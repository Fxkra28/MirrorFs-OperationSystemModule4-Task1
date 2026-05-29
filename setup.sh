#!/bin/bash
apt-get update -qq
apt-get install -y libfuse-dev pkg-config fuse

mkdir -p /home/bagas/reports

echo "Laporan Bulan Januari"  > /home/bagas/reports/laporan_jan.txt
echo "Laporan Bulan Februari" > /home/bagas/reports/laporan_feb.txt
echo "Laporan Bulan Maret"    > /home/bagas/reports/laporan_mar.txt
echo "data,angka,csv"         > /home/bagas/reports/data.csv
echo "Summary laporan"        > /home/bagas/reports/summary.pdf

mkdir -p /mnt/mirror

if ! grep -q "^user_allow_other" /etc/fuse.conf 2>/dev/null; then
    echo "user_allow_other" >> /etc/fuse.conf
fi