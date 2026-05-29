#!/bin/bash
#test.sh
#Jalankan setelah mirrorfs berjalan di terminal lain

MOUNT="/mnt/mirror"
SOURCE="/home/bagas/reports"
PASS=0
FAIL=0

check() {
    local label="$1"
    local result="$2"
    local expected="$3"
    if [ "$result" = "$expected" ]; then
        echo "[PASS] $label"
        PASS=$((PASS+1))
    else
        echo "[FAIL] $label"
        echo "       Expected : $expected"
        echo "       Got      : $result"
        FAIL=$((FAIL+1))
    fi
}

echo "============================================"
echo " MirrorFS Test Suite"
echo "============================================"
echo ""

echo "--- TEST a: Mirroring ---"
# File .txt harus muncul di mount point
check "laporan_jan.txt terlihat di mount" \
    "$(ls $MOUNT | grep -c 'laporan_jan')" "1"
check "laporan_feb.txt terlihat di mount" \
    "$(ls $MOUNT | grep -c 'laporan_feb')" "1"
check "laporan_mar.txt terlihat di mount" \
    "$(ls $MOUNT | grep -c 'laporan_mar')" "1"
echo ""

echo "--- TEST b: Read-Only ---"
#touch harus gagal
touch $MOUNT/test_baru.txt 2>/dev/null
check "touch gagal (file tidak terbuat)" \
    "$(ls $MOUNT | grep -c 'test_baru')" "0"

#echo redirect harus gagal
echo "tes" > $MOUNT/laporan_jan.txt 2>/dev/null
check "write gagal (isi file tidak berubah)" \
    "$(cat $MOUNT/LAPORAN_laporan_jan.txt)" "Laporan Bulan Januari"

#rm harus gagal
rm -f $MOUNT/LAPORAN_laporan_jan.txt 2>/dev/null
check "rm gagal (file masih ada)" \
    "$(ls $MOUNT | grep -c 'LAPORAN_laporan_jan')" "1"
echo ""

echo "--- TEST c: Prefix LAPORAN_ ---"
#File harus muncul dengan prefix
check "prefix LAPORAN_ ada di laporan_jan.txt" \
    "$(ls $MOUNT | grep -c 'LAPORAN_laporan_jan.txt')" "1"
check "prefix LAPORAN_ ada di laporan_feb.txt" \
    "$(ls $MOUNT | grep -c 'LAPORAN_laporan_feb.txt')" "1"

#Isi file harus benar (baca dari file asli)
check "cat LAPORAN_laporan_jan.txt isinya benar" \
    "$(cat $MOUNT/LAPORAN_laporan_jan.txt)" "Laporan Bulan Januari"
check "cat LAPORAN_laporan_feb.txt isinya benar" \
    "$(cat $MOUNT/LAPORAN_laporan_feb.txt)" "Laporan Bulan Februari"
echo ""

echo "--- TEST d: Filter Ekstensi ---"
#.csv tidak boleh muncul
check ".csv tidak muncul di ls" \
    "$(ls $MOUNT | grep -c '\.csv')" "0"

#.pdf tidak boleh muncul
check ".pdf tidak muncul di ls" \
    "$(ls $MOUNT | grep -c '\.pdf')" "0"

#Akses .csv harus gagal
cat $MOUNT/LAPORAN_data.csv 2>/dev/null
check "cat LAPORAN_data.csv gagal (file tersembunyi)" \
    "$(cat $MOUNT/LAPORAN_data.csv 2>/dev/null; echo $?)" "2"
echo ""

echo "============================================"
echo " HASIL: $PASS PASS, $FAIL FAIL"
echo "============================================"