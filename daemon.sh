#/bin/bash

PREFIX=$2
if [ -z "$PREFIX" ]; then
	PREFIX="/usr/local"
fi

TPROOT=$3
if [ -z "$TPROOT" ]; then
	TPROOT="/var/lib/teapotnet"
fi

install_all()
{
	groupadd teapotnet
	useradd -g teapotnet -d $TPROOT -s /bin/bash teapotnet
        mkdir -p $TPROOT
	chown -R teapotnet.teapotnet $TPROOT
	chmod 750 $TPROOT
}

uninstall_all()
{
	rm -rf $TPROOT
        userdel teapotnet
}

if which systemctl &> /dev/null; then
	case "$1" in
	"install")
		install_all
		sed "s/\/usr\/bin\/teapotnet/$(echo $PREFIX | sed -e 's/\//\\\//g')\/bin\/teapotnet/g" teapotnet.service | sed "s/\/var\/lib\/teapotnet/$(echo $TPROOT | sed -e 's/\//\\\//g')/g" - > /etc/systemd/system/teapotnet.service
		echo "Run \"systemctl start teapotnet.service\" to start the daemon and go to http://localhost:8480/"
		echo "Run \"systemctl enable teapotnet.service\" to start it at each boot"
	;;
	"uninstall")
		rm -rf /etc/systemd/system/teapotnet.service
		uninstall_all
	;;
	*)
		echo "Unknown operation. Usage: $0 (install|uninstall) [prefix]"
	esac
elif [ -d /etc/init.d ]; then
	case "$1" in
        "install")
		install_all
                sed "s/\/usr\/bin\/teapotnet/$(echo $PREFIX | sed -e 's/\//\\\//g')\/bin\/teapotnet/g" teapotnet.init | sed "s/\/var\/lib\/teapotnet/$(echo $TPROOT | sed -e 's/\//\\\//g')/g" - > /etc/init.d/teapotnet
                chmod +x /etc/init.d/teapotnet
		update-rc.d teapotnet defaults
		echo "Run \"/etc/init.d/teapotnet start\" to start the daemon and go to http://localhost:8480/"
        ;;
        "uninstall")
		update-rc.d -f teapotnet remove
                rm -rf /etc/init.d/teapotnet
        	uninstall_all
	;;
        *)
                echo "Unknown operation. Usage: $0 (install|uninstall) [prefix]"
        esac
else
	if [ "$1" == "install" ]; then
		echo "This system does not use systemd or init.d, you should set up teapotnet to start at boot time by yourself."
		echo "Launch teapotnet in the appropriate directory then go to http://localhost:8480/"
	fi
fi

