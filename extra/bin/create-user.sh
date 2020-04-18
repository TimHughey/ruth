#!/bin/env /bin/zsh

sudo groupadd -g 5002 -f ruth 
sudo useradd -u 5002 -g 5002 -d /home/ruth -s /bin/zsh -c "Ruth" -m ruth 

sudo usermod -G ruth thughey

if [[ -d  ${HOME}/devel/ruth-home/extra/zsh ]]; then
	cd ${HOME}/devel/ruth-home/extra/zsh
	sudo rsync --chown ruth:ruth -av .zshrc .zshenv /home/ruth
fi
