#!/bin/sh

echo "Building python protobufs..."

protoc -I=src/packets/ --python_out=argali_tether/packets/ src/packets/*.proto
