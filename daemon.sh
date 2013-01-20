#/bin/bash

PREFIX=$2
if [ -z "$PREFIX" ]; then
	PREFIX="/usr/local"
fi

TPROOT="/var/lib/teapotnet"

if which systemctl &> /dev/null; then
	case "$1" in
	"install")
		groupadd teapotnet
		useradd -g teapotnet -d $TPROOT -s /bin/bash teapotnet
		mkdir -p $TPROOT
		chown -R teapotnet.teapotnet $TPROOT
		chmod 750 $TPROOT
	
		cat > /etc/systemd/system/teapotnet.service << EOF
[Unit]
Description=TeapotNet
After=local-fs.target network.target

[Service]
Type=simple
User=teapotnet
Group=teapotnet
WorkingDirectory=/var/lib/teapotnet
ExecStart=$PREFIX/bin/teapotnet

[Install]
WantedBy=multi-user.target
EOF

		echo "Run \"systemctl start teapotnet.service\" to start the daemon and go to http://localhost:8080/"
		echo "Run \"systemctl enable teapotnet.service\" to start it at each boot"
	;;
	"uninstall")
		rm -rf /etc/systemd/system/teapotnet.service
		rm -rf $TPROOT
		userdel teapotnet
	;;
	*)
		echo "Unknown operation. Usage: $0 (install|uninstall) [prefix]"
	esac
else
	if [ "$1" == "install" ]; then
		echo "This system does not use systemd, you should set up teapotnet to start at boot time by yourself."
		echo "Launch teapotnet in the appropriate directory then go to http://localhost:8080/"
	fi
fi

