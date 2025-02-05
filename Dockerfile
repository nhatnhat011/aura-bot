FROM alpine:3.13.1 AS build
COPY . /aura-bot
ADD https://github.com/ladislav-zezula/StormLib/archive/refs/tags/v9.30.tar.gz /aura-bot/stormlib.tar.gz
RUN apk add build-base gmp-dev m4 bzip2-dev zlib-dev cmake && tar -xvf /aura-bot/stormlib.tar.gz &&\
    cd /aura-bot/StormLib-9.30 && cmake CMakeLists.txt && make && make install &&\
    cd /aura-bot/bncsutil/src/bncsutil/ && make &&\
    cd /aura-bot/ && make &&\
    chmod +x entry.sh
FROM alpine:latest
COPY --from=build /aura-bot/StormLib-9.30/libstorm.a /aura-bot/bncsutil/src/bncsutil/libbncsutil.so /usr/lib
COPY --from=build /aura-bot/aura++ /aura-bot/ip-to-country.csv /aura-bot/aura_example.cfg /aura-bot/entry.sh /app/
COPY --from=build /aura-bot/wc3 /app/wc3
COPY --from=build /aura-bot/mapcfgs /app/mapcfgs
RUN apk add libbz2 gmp libstdc++ python3 py3-pip &&\
    pip install gdown --break-system-packages
WORKDIR /app
CMD ["./entry.sh"]
