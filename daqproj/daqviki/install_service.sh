#!/bin/bash

sudo cp daqviki.service /lib/systemd/system/.
sudo chmod 644 /lib/systemd/system/daqviki.service
sudo systemctl daemon-reload
sudo systemctl enable daqviki.service


