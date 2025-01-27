FROM alpine:3.13.1 AS build
RUN apk add git build-base gmp-dev m4 bzip2-dev zlib-dev cmake &&\
    git clone https://github.com/nhatnhat011/aura-bot.git &&\
    mkdir aura-bot/StormLib/build && cd aura-bot/StormLib/build &&\
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DYNAMIC_MODULE=1 .. && make && mak>
    cd /aura-bot/bncsutil/src/bncsutil/ && make &&\
    cd /aura-bot/ && make &&\
    chmod +x entry.sh
FROM alpine:latest
COPY --from=build /aura-bot/StormLib/build/libstorm.a /aura-bot/bncsutil/src/bn>
COPY --from=build /aura-bot/aura++ /aura-bot/ip-to-country.csv /aura-bot/aura_e>
RUN apk add libbz2 gmp libstdc++ python3 py3-pip &&\
    pip install gdown --break-system-packages
WORKDIR /app
CMD ["./entry.sh"]
