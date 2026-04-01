# ---- Build Stage ----
FROM debian:bullseye-slim AS builder

RUN apt-get update && apt-get install -y \
	g++ \
	make \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY Makefile .
COPY src/ src/

RUN make -j$(nproc)

# ---- Runtime Stage ----
FROM debian:bullseye-slim AS runtime

WORKDIR /app

COPY --from=builder /build/ircserv .
COPY conf/ conf/

EXPOSE ${IRC_PORT}

CMD ["sh", "-c", "./ircserv ${IRC_PORT} ${IRC_PASSWORD}"]