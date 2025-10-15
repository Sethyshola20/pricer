# Pricer

A full-stack, containerized Black-Scholes option pricer with a modern React UI, a WebSocket/Proxy backend, and a high-performance C++ pricing engine.

---

## Project Structure

```
pricer/
├── pricer-cpp/      # C++ Black-Scholes pricing daemon (TCP server)
│   ├── Dockerfile
│   └── pricer_server.cpp
├── proxy/           # WebSocket-to-TCP proxy (Node/Bun)
│   ├── Dockerfile
│   ├── proxy.ts
│   ├── package.json
│   └── tsconfig.json
├── ui-react/        # React 19 + shadcn/ui frontend
│   ├── Dockerfile
│   ├── package.json
│   ├── src/
│   │   ├── components/
│   │   ├── hooks/
│   │   ├── lib/
│   │   ├── routes/
│   │   ├── types/
│   │   └── use-cases/
│   └── public/
├── docker-compose.yml
```

---

## Quick Start

### Prerequisites

- [Docker](https://www.docker.com/)
- [Docker Compose](https://docs.docker.com/compose/)
- (For local dev) [Node.js](https://nodejs.org/) and [Bun](https://bun.sh/) if you want to run proxy outside Docker

---

### 1. Build and Run All Services

From the root of the repo:

```sh
docker compose up --build
```

- The C++ pricer will listen on port `9000`.
- The proxy will listen on port `8080` (WebSocket).
- The React UI will be available at [http://localhost:5173](http://localhost:5173).

---

### 2. Usage

- Open [http://localhost:5173](http://localhost:5173) in your browser.
- Use the UI to price European call/put options using the Black-Scholes model.
- The UI communicates with the backend via WebSocket, which proxies requests to the C++ daemon.

---

## Components

### pricer-cpp

- **Language:** C++
- **Role:** High-performance TCP server for Black-Scholes pricing.
- **Build:** Dockerfile compiles `pricer_server.cpp` with Boost.Asio.
- **Protocol:** Receives 41-byte binary requests, sends 24-byte binary responses.

### proxy

- **Language:** TypeScript (Bun/Node)
- **Role:** WebSocket server for the frontend, TCP client for the C++ daemon.
- **Build:** Dockerfile for Bun/Node.
- **Protocol:** Converts JSON WebSocket messages to binary TCP requests and vice versa.

### ui-react

- **Language:** React 19, TypeScript, shadcn/ui, Tailwind CSS
- **Role:** User interface for entering option parameters and viewing results.
- **Build:** Dockerfile for Vite dev server.
- **Features:** Modern form, sliders, tabs, toast notifications, live pricing.

---

## Development

### Hot Reloading

- **ui-react:** Source is mounted as a Docker volume for instant reloads.
- **proxy:** Source is mounted as a Docker volume; restart container for code changes or use `bun --hot` locally.
- **pricer-cpp:** Source is mounted; rebuild container to apply C++ code changes.

### Running Proxy Locally

If you want to run the proxy outside Docker for debugging:

```sh
cd proxy
bun run proxy.ts
```
Set `PRICER_HOST=localhost` if running the C++ daemon locally.

---

## Protocol Details

### Request (41 bytes)

| Field   | Type    | Offset | Size |
|---------|---------|--------|------|
| S       | double  | 0      | 8    |
| K       | double  | 8      | 8    |
| r       | double  | 16     | 8    |
| sigma   | double  | 24     | 8    |
| T       | double  | 32     | 8    |
| type    | uint8_t | 40     | 1    |

### Response (24 bytes)

| Field   | Type    | Offset | Size |
|---------|---------|--------|------|
| price   | double  | 0      | 8    |
| delta   | double  | 8      | 8    |
| vega    | double  | 16     | 8    |

---

## Troubleshooting

- **UI not updating:** Make sure Docker volumes are mounted and the dev server is running.
- **NaN/null results:** Check that the proxy sends exactly 41 bytes and the C++ server reads all 41 bytes before unpacking.
- **CORS errors:** The UI uses WebSocket, not HTTP fetch. Ensure you connect via `ws://localhost:8080/`.
- **TCP connection closes early:** The proxy should keep the TCP connection open for the lifetime of the WebSocket session.

