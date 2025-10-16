#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include <cmath>
#include <memory>
#include <cstring>

using boost::asio::ip::tcp;

enum class OptionType : uint8_t { Call = 0, Put = 1 };

struct BinomialParams : BSParams {
    uint32_t steps;
};

struct BinomialResult : BSResult {};

BinomialResult binomial_tree_price(const BinomialParams& p) {
    const double dt = p.T / p.steps;
    const double u = std::exp(p.sigma * std::sqrt(dt));
    const double d = 1.0 / u;
    const double disc = std::exp(-p.r * dt);
    const double q = (std::exp(p.r * dt) - d) / (u - d);

    std::vector<double> prices(p.steps + 1);
    std::vector<double> option(p.steps + 1);

    for (size_t i = 0; i <= p.steps; ++i) {
        double ST = p.S * std::pow(u, p.steps - i) * std::pow(d, i);
        option[i] = (p.type == OptionType::Call) ? std::max(0.0, ST - p.K) : std::max(0.0, p.K - ST);
    }

    for (int step = p.steps - 1; step >= 0; --step) {
        for (int i = 0; i <= step; ++i) {
            option[i] = disc * (q * option[i] + (1 - q) * option[i + 1]);
        }
    }

    double delta = (option[1] - option[0]) / (p.S * (u - d));
    return { option[0], delta, 0.0 };
}


struct BSParams { double S, K, r, sigma, T; OptionType type; uint32_t steps = 0; };
struct BSResult { double price, delta, vega; };

std::ostream& operator<<(std::ostream& os, const BSParams& p) {
    os << "{ S: " << p.S
       << ", K: " << p.K
       << ", r: " << p.r
       << ", sigma: " << p.sigma
       << ", T: " << p.T
       << ", type: " << (p.type == OptionType::Call ? "Call" : "Put")
       << " }";
    return os;
}

static inline double norm_cdf(double x) {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
}

static inline double norm_pdf(double x) {
    static const double inv_sqrt_2pi = 0.3989422804014327;
    return inv_sqrt_2pi * std::exp(-0.5 * x * x);
}

BSResult black_scholes(const BSParams& p) {
    if (p.S <= 0.0 || p.K <= 0.0 || p.T < 0.0 || p.sigma < 0.0) {
        return {0.0, 0.0, 0.0};
    }

    if (p.T == 0.0) {
        double intrinsic = (p.type == OptionType::Call) ? std::max(0.0, p.S - p.K) : std::max(0.0, p.K - p.S);
        double delta = (p.type == OptionType::Call) ? ((p.S > p.K) ? 1.0 : 0.0) : ((p.S < p.K) ? -1.0 : 0.0);
        return {intrinsic, delta, 0.0};
    }

    double sqrtT = std::sqrt(p.T);
    double d1 = (std::log(p.S / p.K) + (p.r + 0.5 * p.sigma * p.sigma) * p.T) / (p.sigma * sqrtT);
    double d2 = d1 - p.sigma * sqrtT;
    double discK = p.K * std::exp(-p.r * p.T);

    double price = 0.0;
    if (p.type == OptionType::Call)
        price = p.S * norm_cdf(d1) - discK * norm_cdf(d2);
    else
        price = discK * norm_cdf(-d2) - p.S * norm_cdf(-d1);

    double delta = (p.type == OptionType::Call) ? norm_cdf(d1) : (norm_cdf(d1) - 1.0);
    double vega = p.S * norm_pdf(d1) * sqrtT;

    return {price, delta, vega};
}

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket) : socket_(std::move(socket)) {}
    void start() { do_read(); }

private:
    tcp::socket socket_;
    std::array<char, 45> reqbuf_; 
    std::array<char, 24> resbuf_; 

    void do_read() {
        auto self = shared_from_this();
        boost::asio::async_read(socket_, boost::asio::buffer(reqbuf_),
            [this, self](boost::system::error_code ec, std::size_t bytes_transferred ) {
                if (!ec && bytes_transferred == 45) {
                    std::cout << "Raw request bytes: ";
                    for (size_t i = 0; i < 45; ++i) {
                        printf("%02x ", static_cast<unsigned char>(reqbuf_[i]));
                    }
                    std::cout << std::endl;

                    BSParams p;
                    std::memcpy(&p.S, reqbuf_.data() + 0, 8);
                    std::memcpy(&p.K, reqbuf_.data() + 8, 8);
                    std::memcpy(&p.r, reqbuf_.data() + 16, 8);
                    std::memcpy(&p.sigma, reqbuf_.data() + 24, 8);
                    std::memcpy(&p.T, reqbuf_.data() + 32, 8);
                    uint8_t t = static_cast<uint8_t>(reqbuf_[40]);
                    p.type = (t == 0) ? OptionType::Call : OptionType::Put;

                    uint32_t steps;
                    std::memcpy(&steps, reqbuf_.data() + 41, 4);

                    std::cout << "Decoded params: " << p << std::endl;

                    if(steps > 0) {
                        BinomialResult out = binomial_tree_price(p);
                        std::memcpy(resbuf_.data() + 0, &out.price, 8);
                        std::memcpy(resbuf_.data() + 8, &out.delta, 8);
                        std::memcpy(resbuf_.data() + 16, &out.vega, 8);
                    }else {
                        BSResult out = black_scholes(p);
                        std::memcpy(resbuf_.data() + 0, &out.price, 8);
                        std::memcpy(resbuf_.data() + 8, &out.delta, 8);
                        std::memcpy(resbuf_.data() + 16, &out.vega, 8);
                    }

                    do_write();
                } else {
                     std::cerr << "Read error or incomplete request: " << ec.message()
                               << " (" << bytes_transferred << " bytes read)" << std::endl;
                }
            });
    }

    void do_write() {
        auto self = shared_from_this();
        boost::asio::async_write(socket_, boost::asio::buffer(resbuf_),
            [this, self](boost::system::error_code ec, std::size_t ) {
                if (!ec) {
                    do_read();
                } else {
                     std::cerr << "Write error: " << ec.message() << "\n";
                }
            });
    }
};

class Server {
public:
    Server(boost::asio::io_context& io_context, uint16_t port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    tcp::acceptor acceptor_;
    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket))->start();
            }
            do_accept();
        });
    }
};


int main(int argc, char* argv[]) {
    try {
        uint16_t port = 9000;
        if (argc > 1) port = static_cast<uint16_t>(std::stoi(argv[1]));
        boost::asio::io_context io_context;
        Server s(io_context, port);
        std::cout << "Pricer daemon listening on port " << port << "\n";
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}