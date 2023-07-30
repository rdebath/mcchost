# Official style Docker file.
#
# This uses small layers in a effort to reduce the slow build time;
# it's still very slow but much faster than a full rebuild.
# To preserve the small image size the "FROM scratch" section is required.

# The user id that the server will run as.
ARG UID=1000
ARG GID=1000
ARG MPATH=/home/user

# Tested with this version; should be fine with later versions.
ARG FROM=alpine:3.18

# Add options and controls to the make command.
ARG MAKEARGS=

# Add gdb here, if you want to make the image seven times larger.
# Note: GDB seems to have a problem with the Alpine abort() functon.
ARG EPACKS=

################################################################################
# Useful commands.
# docker run --name=mcchost --rm -it -e PORT=25565 -p 25565:25565 -v "$(pwd)/mcchost":/home/user mcchost
# docker run --name=mcchost -d -e PORT=25565 -p 25565:25565 -v mcchost:/home/user mcchost
#
# The /home/user directory will be mcchost's current directory.
# MCCHost will run as user id 1000. (ARG UID)
#
# Ctrl-P Ctrl-Q
# docker attach mcchost
# docker exec -it mcchost sh
# docker exec -it -u 0 mcchost sh
# docker logs -n 20 mcchost
################################################################################

FROM $FROM AS build

ARG EPACKS
RUN apk add --no-cache -t build-packages build-base gdb git \
	curl tini zlib-dev lmdb-dev $EPACKS
RUN apk add --no-cache -t run-packages --repositories-file /dev/null \
	curl tini zlib lmdb $EPACKS

ARG MAKEARGS
WORKDIR /usr/src
COPY . .
RUN make $MAKEARGS INSTALLER=install INSTDIR=/usr/local/bin install

WORKDIR /root
RUN rm -rf /usr/src
RUN apk del --repositories-file /dev/null build-packages

ARG UID
ARG GID
ARG MPATH
RUN addgroup -g $GID group
RUN adduser user -u $UID -G group -h $MPATH -D

################################################################################
FROM scratch
COPY --from=0 / /
WORKDIR /root

ARG UID
ARG GID
ARG MPATH
USER $UID:$GID
WORKDIR $MPATH

ENTRYPOINT ["/sbin/tini", "--"]
CMD mcchost-server -logstderr -no-detach -cwd -net ${PORT:+-port $PORT}
