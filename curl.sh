#!/bin/sh -

salt=RQGUYDMZJWGE4TCMRRMFRCAIBNBI
name="Some%20Random%20Server"

PREFIX="port=25565&max=42&public=True&version=7&software=%262Custom"
usercount=1

URL=http://www.classicube.net/heartbeat.jsp

exec curl -s "$URL?$PREFIX&name=$name&salt=$salt&users=$usercount"
