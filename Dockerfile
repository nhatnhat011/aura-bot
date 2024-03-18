FROM alpine:3.13.1 as build
WORKDIR /src
RUN apk add git build-base gmp-dev m4 bzip2-dev zlib-dev cmake &&\
    git clone https://github.com/nhatnhat011/aura-bot.git &&\
    mkdir aura-bot/StormLib/build && cd aura-bot/StormLib/build &&\
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DYNAMIC_MODULE=1 .. && make && make install &&\
    cd /src/aura-bot/bncsutil/src/bncsutil/ && make &&\
    cd /src/aura-bot/ && make
FROM alpine:latest
COPY --from=build /src/aura-bot/StormLib/build/libstorm.a /src/aura-bot/bncsutil/src/bncsutil/libbncsutil.so /usr/lib
COPY --from=build /src/aura-bot/aura++ /src/aura-bot/ip-to-country.csv /app/
RUN apk add libbz2 gmp libstdc++
WORKDIR /app
CMD ["./aura++","./aura-bot/aura.cfg"]
