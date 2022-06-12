# Note I'm not including gdb in the image because Alpine strips everything.

FROM alpine:3.13
# Manually create the mount path because Docker can't do it right.
# It will copy these perms to a new volume, but not to a new host
# directory.

ARG UID=1000
ARG GID=1000
ARG MPATH=/home/user

RUN set -eu;_() { echo "$@";};(\
_ 'apk add --no-cache -t build-packages \';\
_ '    build-base zlib-dev git curl tini';\
_ 'apk add --no-cache -t run-packages --repositories-file /dev/null \';\
_ '    zlib curl tini';\
_ '';\
_ 'git clone https://github.com/rdebath/mcchost.git mcchost';\
_ 'make -j -C mcchost INSTDIR=/usr/local/bin install';\
_ 'rm -rf mcchost';\
_ 'apk del --repositories-file /dev/null build-packages';\
_ '';\
_ 'mkdir -p -m 744 "$MPATH"';\
_ 'chown $UID:$GID "$MPATH"';\
)>/tmp/install;sh -e /tmp/install;rm -f /tmp/install

USER $UID:$GID
WORKDIR $MPATH
ENTRYPOINT ["/sbin/tini", "--"]
CMD ["mcchost-server"]
