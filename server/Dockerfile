# The Cubey server on Fly.io (fly.toml at the repo root). Build context: the repo root.
FROM oven/bun:1.4-slim
WORKDIR /app/server
COPY server/package.json server/bun.lock ./
RUN bun install --frozen-lockfile --production
COPY server/src ./src
COPY server/scripts ./scripts
COPY content /app/content
ENV NODE_ENV=production
EXPOSE 8788
CMD ["bun", "src/server.ts"]
