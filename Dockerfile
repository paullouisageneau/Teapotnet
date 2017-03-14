FROM debian:stable
MAINTAINER Paul-Louis Ageneau <paul-louis (at) ageneau (dot) org>
EXPOSE 8480 8080

RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install -y git build-essential debhelper dh-systemd libgnutls28-dev nettle-dev

RUN git clone https://github.com/paullouisageneau/Teapotnet.git /tmp/teapotnet
RUN cd /tmp/teapotnet && dpkg-buildpackage -b -us -uc
RUN rm -r /tmp/teapotnet
RUN dpkg -i /tmp/teapotnet_*.deb

RUN mkdir -p /var/lib/teapotnet
RUN chown -R teapotnet.teapotnet /var/lib/teapotnet
RUN chmod 750 /var/lib/teapotnet
VOLUME /var/lib/teapotnet

WORKDIR /var/lib/teapotnet 
USER teapotnet
ENTRYPOINT /usr/bin/teapotnet

