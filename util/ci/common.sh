#!/bin/bash -e

# Linux build only
install_linux_deps() {
	# local pkgs=(
		# cmake gettext postgresql doxygen 
		# libpng-dev libjpeg-dev libxi-dev libgl1-mesa-dev
		# libsqlite3-dev libhiredis-dev libogg-dev libgmp-dev libvorbis-dev
		# libopenal-dev libpq-dev libleveldb-dev libcurl4-openssl-dev libzstd-dev
	# )
	
	local pkgs=(
		cmake gettext pkg-config 
		libpng-dev libjpeg-dev libxi-dev libgl1-mesa-dev
		libsqlite3-dev libhiredis-dev libogg-dev libgmp-dev libvorbis-dev
		libopenal-dev libleveldb-dev libcurl4-openssl-dev libzstd-dev libluajit-5.1-dev libmysqlclient-dev 
	)

	if [[ "$1" == "--no-irr" ]]; then
		shift
	else
		local ver=$(cat misc/irrlichtmt_tag.txt)
		wget "https://github.com/district53/irrlicht/releases/download/$ver/ubuntu-focal.tar.gz"
		sudo tar -xaf ubuntu-focal.tar.gz -C /usr/local
	fi

	sudo apt-get update
	sudo apt-get install -y --no-install-recommends "${pkgs[@]}" "$@"

	# sudo systemctl start postgresql.service
	# sudo -u postgres psql <<<"
		# CREATE USER minetest WITH PASSWORD 'minetest';
		# CREATE DATABASE minetest;
	# "
}

# macOS build only
install_macos_deps() {
	local pkgs=(
		cmake gettext freetype gmp jpeg-turbo jsoncpp leveldb
		libogg libpng libvorbis luajit zstd pkg-config mysql-client
		openssl mysql-client
	)
	export HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1
	export HOMEBREW_NO_INSTALL_CLEANUP=1
	# contrary to how it may look --auto-update makes brew do *less*
	brew update --auto-update
	brew install --display-times "${pkgs[@]}"
	brew unlink $(brew ls --formula)
	brew link "${pkgs[@]}" --force
}
