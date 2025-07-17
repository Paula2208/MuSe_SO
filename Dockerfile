FROM ubuntu:22.04

RUN apt-get update && apt-get install -y gcc

WORKDIR /app
COPY . .

RUN chmod +x start.sh
CMD ["./start.sh"]
