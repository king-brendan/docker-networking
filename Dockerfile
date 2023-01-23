FROM ubuntu:latest

RUN apt-get update
RUN apt-get install -y gcc

ADD prj1.c /app/
ADD hostfile.txt /app/
WORKDIR /app/
RUN gcc prj1.c -lpthread -o prj1

ENTRYPOINT /app/prj1
