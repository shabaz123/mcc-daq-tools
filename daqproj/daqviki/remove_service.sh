#!/bin/bash

sudo systemctl stop daqviki.service
sudo systemctl disable daqviki.service
sudo rm /lib/systemd/system/daqviki.service


