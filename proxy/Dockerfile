FROM oven/bun:latest

WORKDIR /app

COPY package.json /app/
COPY proxy.ts /app/


RUN bun install

EXPOSE 8080

CMD ["bun", "run", "start"]
