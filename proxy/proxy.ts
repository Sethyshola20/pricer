import WebSocket, { WebSocketServer } from "ws";
import net from "net";

const WS_PORT = Number(process.env.WS_PORT || 8080);
const PRICER_HOST = process.env.PRICER_HOST || "pricer-cpp"; 
const PRICER_PORT = Number(process.env.PRICER_PORT || 9000);

type OptionParams = {
    spot: number;
    strike: number;
    rate: number;
    volatility: number;
    maturity: number;
    steps: number
    type: "call" | "put";
}

console.log("PRICER_HOST",PRICER_HOST)
console.log(`Starting Bun proxy. WS_PORT=${WS_PORT}, PRICER=${PRICER_HOST}:${PRICER_PORT}`);

const wss = new WebSocketServer({ port: WS_PORT });

wss.on("listening", () => {
  console.log(`WebSocket server listening on ws://0.0.0.0:${WS_PORT}`);
});

wss.on("connection", (ws) => {
  const socket = new net.Socket();
  socket.connect(PRICER_PORT, PRICER_HOST, () => {
    console.log("Connected to pricer daemon");
  });

  let responseBuffer = Buffer.alloc(0);

  socket.on("data", (data: Buffer) => {
    responseBuffer = Buffer.concat([responseBuffer, data]);
    while (responseBuffer.length >= 24) {
      const chunk = responseBuffer.slice(0, 24);
      responseBuffer = responseBuffer.slice(24);

      const price = chunk.readDoubleLE(0);
      const delta = chunk.readDoubleLE(8);
      const vega = chunk.readDoubleLE(16);

      const msg = {
        type: "price_result",
        data: {
          price: Number.isFinite(price) ? price : null,
          delta: Number.isFinite(delta) ? delta : null,
          vega: Number.isFinite(vega) ? vega : null,
          ts_server: Date.now()
        }
      };
      try {
        ws.send(JSON.stringify(msg));
      } catch (_) {}
    }
  });

  socket.on("error", (err) => {
    console.error("TCP socket error:", err?.message ?? err);
    try { ws.close(); } catch (_) {}
  });

  ws.on("message", (message: WebSocket.Data) => {
    try {
      const text = (typeof message === "string") ? message : message.toString();
      const req = JSON.parse(text) as OptionParams;
      
      const S = Number(req.spot);
      const K = Number(req.strike);
      const r = Number(req.rate);
      const sigma = Number(req.volatility);
      const T = Number(req.maturity);
      const type = (req.type === "put") ? 1 : 0;

      const buf = Buffer.alloc(req.steps ? 45 : 41);
      buf.writeDoubleLE(S, 0);
      buf.writeDoubleLE(K, 8);
      buf.writeDoubleLE(r, 16);
      buf.writeDoubleLE(sigma, 24);
      buf.writeDoubleLE(T, 32);
      buf.writeUInt8(type, 40);
      if(req.steps) buf.writeUInt32LE(req.steps, 41);

      socket.write(buf, (err) => {
        if (err) {
          console.error("Failed to write to pricer:", err);
          try { ws.send(JSON.stringify({ type: "error", message: "Failed to send to pricer" })); } catch (_) {}
        }else{
          console.log(buf.byteLength)
        }
      });
    } catch (e) {
      const errMsg = { type: "error", message: "bad request or invalid JSON" };
      try { ws.send(JSON.stringify(errMsg)); } catch (_) {}
    }
  });

  ws.on("close", () => {
    console.log("WebSocket closed, ending TCP socket");
    try { socket.end(); } catch (_) {}
  });

  ws.on("error", () => {
    try { socket.destroy(); } catch (_) {}
  });
});

wss.on("error", (err: unknown) => {
  console.error("WebSocket server error:", err);
});