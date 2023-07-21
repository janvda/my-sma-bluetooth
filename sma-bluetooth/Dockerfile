FROM alpine

RUN apk update && \
     apk add alpine-sdk bluez-dev libxml2-dev mariadb-dev

RUN mkdir /src
COPY src/* /src
RUN cd /src; make &&  make install

ENTRYPOINT ["/bin/sleep","3600"]
