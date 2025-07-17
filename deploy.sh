#!/bin/bash

echo -e "\n Deleting existing instance if it exists..."

gcloud compute instances delete misocket-server --zone=us-central1-a


echo -e "\n Creating a new instance..."

gcloud compute instances create misocket-server \
  --zone=us-central1-a \
  --machine-type=e2-medium \
  --image-family=ubuntu-2204-lts \
  --image-project=ubuntu-os-cloud \
  --tags=socket-server \
  --metadata startup-script='#! /bin/bash
    apt update
    apt install -y make gcc wget unzip
    cd /home
    git clone https://github.com/YOUR_USER/YOUR_REPO.git
    cd YOUR_REPO
    chmod +x start.sh
    ./start.sh'

echo -e "\n Allowing firewall rules for the socket server..."

gcloud compute firewall-rules create allow-socket-server \
  --allow tcp:3550 \
  --target-tags=socket-server \
  --source-ranges=0.0.0.0/0 \
  --description="Permitir tráfico al puerto 3550"

echo -e "\n Instance created and firewall rules set up. You can now connect to your server."

gcloud compute instances list
