# MirrorFS

Bagas adalah seorang arsiparis digital yang sangat rapi. Dia memiliki satu folder berisi ratusan file laporan kerja, namun dia menyadari ada dua masalah:

1. Nama file-filenya tidak seragam, misalnya `laporan_keuangan_final_v2_REVISI.txt`
2. Rekan kerjanya sering tidak sengaja mengubah atau menghapus file laporan tersebut

Sebagai mahasiswa Sistem Operasi, Bagas memutuskan untuk membuat **MirrorFS**, sebuah FUSE filesystem yang "mencerminkan" folder laporannya dengan tampilan yang lebih rapi dan aman.

## Deskripsi Tugas

Buatlah program FUSE bernama `mirrorfs.c` yang di-mount pada `/mnt/mirror` dan membaca dari source directory `/home/bagas/reports`. Filesystem ini harus menerapkan beberapa perilaku khusus.

> **Catatan:** Setup awal sudah disiapkan dalam `setup.sh`. Cukup jalankan sekali sebelum mengerjakan.

### a. Mirroring Source Directory

FUSE filesystem harus menampilkan isi dari `/home/bagas/reports` secara langsung. Ketika user menjalankan `ls /mnt/mirror`, harus menampilkan file yang sama dengan yang ada di source directory.

```bash
$ ls /home/bagas/reports
laporan_jan.txt  laporan_feb.txt  laporan_mar.txt  data.csv

$ ls /mnt/mirror
laporan_jan.txt  laporan_feb.txt  laporan_mar.txt  data.csv
```

### b. Read-Only Filesystem

Seluruh filesystem bersifat **read-only**. Semua operasi write harus gagal dan mengembalikan error `EROFS`.

```bash
$ touch /mnt/mirror/file_baru.txt
touch: cannot touch '/mnt/mirror/file_baru.txt': Read-only file system

$ rm /mnt/mirror/laporan_jan.txt
rm: cannot remove '/mnt/mirror/laporan_jan.txt': Read-only file system

$ echo "test" > /mnt/mirror/laporan_jan.txt
bash: /mnt/mirror/laporan_jan.txt: Read-only file system
```

### c. Prefix Nama File

Semua nama file di `/mnt/mirror` harus memiliki prefix `LAPORAN_`. Namun ketika dibaca, tetap harus mengarah ke file aslinya di source directory.

```bash
$ ls /mnt/mirror
LAPORAN_laporan_jan.txt  LAPORAN_laporan_feb.txt  LAPORAN_data.csv

$ cat /mnt/mirror/LAPORAN_laporan_jan.txt
Laporan Bulan Januari     ← isi dari file asli laporan_jan.txt
```

### d. Filter Ekstensi

MirrorFS hanya menampilkan file berekstensi `.txt`. File dengan ekstensi lain tidak boleh muncul maupun bisa diakses.

```bash
$ ls /home/bagas/reports
laporan_jan.txt  laporan_feb.txt  data.csv  summary.pdf

$ ls /mnt/mirror
LAPORAN_laporan_jan.txt  LAPORAN_laporan_feb.txt
# data.csv dan summary.pdf tidak muncul
```

## Contoh Skenario Lengkap

```bash
$ ls /home/bagas/reports
laporan_jan.txt  laporan_feb.txt  laporan_mar.txt  data.csv  summary.pdf

$ ls /mnt/mirror
LAPORAN_laporan_jan.txt  LAPORAN_laporan_feb.txt  LAPORAN_laporan_mar.txt

$ cat /mnt/mirror/LAPORAN_laporan_jan.txt
Laporan Bulan Januari

$ touch /mnt/mirror/test.txt
touch: cannot touch '/mnt/mirror/test.txt': Read-only file system

$ cat /mnt/mirror/LAPORAN_data.csv
cat: /mnt/mirror/LAPORAN_data.csv: No such file or directory
```

## Notes

- Source directory: `/home/bagas/reports`
- Mount point: `/mnt/mirror`
- Jalankan `setup.sh` terlebih dahulu
- Debugging: `./mirrorfs -f /mnt/mirror`
- Compile: `gcc -Wall $(pkg-config fuse --cflags) mirrorfs.c -o mirrorfs $(pkg-config fuse --libs)`
- Unmount: `fusermount -u /mnt/mirror`

---

---

# KUNCI JAWABAN

## 1. `setup.sh` — Persiapan Lingkungan

```bash
#!/bin/bash
#setup.sh

set -e  #Hentikan script jika ada error

echo "=== [1/4] Menginstall dependensi ==="
apt-get update -qq
apt-get install -y libfuse-dev pkg-config fuse

echo "=== [2/4] Membuat source directory ==="
mkdir -p /home/bagas/reports

echo "Laporan Bulan Januari"  > /home/bagas/reports/laporan_jan.txt
echo "Laporan Bulan Februari" > /home/bagas/reports/laporan_feb.txt
echo "Laporan Bulan Maret"    > /home/bagas/reports/laporan_mar.txt
echo "data,angka,csv"         > /home/bagas/reports/data.csv
echo "Summary laporan"        > /home/bagas/reports/summary.pdf

echo "=== [3/4] Membuat mount point ==="
mkdir -p /mnt/mirror

echo "=== [4/4] Mengaktifkan allow_other untuk FUSE ==="
#Izinkan non-root user menggunakan a
if ! grep -q "^user_allow_other" /etc/fuse.conf 2>/dev/null; then
    echo "user_allow_other" >> /etc/fuse.conf
fi

echo ""
echo "=== Setup selesai! ==="
echo "Isi /home/bagas/reports:"
ls -la /home/bagas/reports
echo ""
echo "Langkah selanjutnya:"
echo "  1. gcc -Wall \$(pkg-config fuse --cflags) mirrorfs.c -o mirrorfs \$(pkg-config fuse --libs)"
echo "  2. ./mirrorfs /mnt/mirror"
echo "  3. bash test.sh"
```

## 2. `mirrorfs.c` — Source Code FUSE

```c
#define FUSE_USE_VERSION 28

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SOURCE_DIR   "/home/bagas/reports"
#define PREFIX       "LAPORAN_"
#define PREFIX_LEN   8
#define ALLOWED_EXT  ".txt"

static int is_txt_file(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;
    return strcmp(dot, ALLOWED_EXT) == 0;
}

// buang prefix LAPORAN_ dari path FUSE → balikin path asli di source dir
// "/LAPORAN_laporan_jan.txt"  ->  "/home/bagas/reports/laporan_jan.txt"
static int fuse_path_to_real(const char *fuse_path, char *real_path, size_t size) {
    if (strcmp(fuse_path, "/") == 0) {
        snprintf(real_path, size, "%s", SOURCE_DIR);
        return 0;
    }

    const char *name = fuse_path + 1;   // skip "/"
    if (strncmp(name, PREFIX, PREFIX_LEN) != 0)
        return -1;

    snprintf(real_path, size, "%s/%s", SOURCE_DIR, name + PREFIX_LEN);
    return 0;
}

static int mfs_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode  = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        return 0;
    }

    char real_path[1024];
    if (fuse_path_to_real(path, real_path, sizeof(real_path)) < 0)
        return -ENOENT;

    const char *filename = path + 1 + PREFIX_LEN;
    if (!is_txt_file(filename))
        return -ENOENT;

    if (lstat(real_path, stbuf) == -1)
        return -errno;

    // matiin bit write biar bener-bener read-only di mata sistem
    stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    return 0;
}

static int mfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
    (void) offset; (void) fi;

    // cuma support 1 level (root), no subdir
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    DIR *dp = opendir(SOURCE_DIR);
    if (!dp) return -errno;

    filler(buf, ".",  NULL, 0);
    filler(buf, "..", NULL, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (!is_txt_file(de->d_name))
            continue;

        char display[1024];
        snprintf(display, sizeof(display), "%s%s", PREFIX, de->d_name);
        if (filler(buf, display, NULL, 0)) break;
    }

    closedir(dp);
    return 0;
}

static int mfs_open(const char *path, struct fuse_file_info *fi) {
    // tolak semua mode selain read-only
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EROFS;

    char real_path[1024];
    if (fuse_path_to_real(path, real_path, sizeof(real_path)) < 0)
        return -ENOENT;

    const char *filename = path + 1 + PREFIX_LEN;
    if (!is_txt_file(filename))
        return -ENOENT;

    // test buka dulu, biar error file asli ke-propagate ke user
    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

static int mfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    (void) fi;

    char real_path[1024];
    if (fuse_path_to_real(path, real_path, sizeof(real_path)) < 0)
        return -ENOENT;

    const char *filename = path + 1 + PREFIX_LEN;
    if (!is_txt_file(filename))
        return -ENOENT;

    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}

// semua op write ditolak — read-only filesystem
static int mfs_write(const char *p, const char *b, size_t s, off_t o, struct fuse_file_info *fi) {
    (void) p; (void) b; (void) s; (void) o; (void) fi;
    return -EROFS;
}
static int mfs_create(const char *p, mode_t m, struct fuse_file_info *fi) {
    (void) p; (void) m; (void) fi;
    return -EROFS;
}
static int mfs_mkdir(const char *p, mode_t m)        { (void) p; (void) m; return -EROFS; }
static int mfs_unlink(const char *p)                 { (void) p; return -EROFS; }
static int mfs_rmdir(const char *p)                  { (void) p; return -EROFS; }
static int mfs_rename(const char *f, const char *t)  { (void) f; (void) t; return -EROFS; }
static int mfs_truncate(const char *p, off_t s)      { (void) p; (void) s; return -EROFS; }

static struct fuse_operations mfs_oper = {
    .getattr  = mfs_getattr,
    .readdir  = mfs_readdir,
    .open     = mfs_open,
    .read     = mfs_read,
    .write    = mfs_write,
    .create   = mfs_create,
    .mkdir    = mfs_mkdir,
    .unlink   = mfs_unlink,
    .rmdir    = mfs_rmdir,
    .rename   = mfs_rename,
    .truncate = mfs_truncate,
};

int main(int argc, char *argv[]) {
    umask(0);
    return fuse_main(argc, argv, &mfs_oper, NULL);
}
```

## 3. `test.sh` — Script Pengujian

```bash
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
```

## 4. Cara Menjalankan (x86 dan ARM64)

Script dan source code ini **tidak perlu diubah** untuk x86 maupun ARM64. Perbedaannya hanya pada perintah compile karena FUSE library dikompilasi sesuai arsitektur yang terdeteksi otomatis oleh `pkg-config`.

```bash
#Step 1: Setup (sekali saja)
sudo bash setup.sh

#Step 2: Compile
#Sama persis untuk x86 maupun ARM64 — pkg-config otomatis menyesuaikan
gcc -Wall $(pkg-config fuse --cflags) mirrorfs.c -o mirrorfs $(pkg-config fuse --libs)

#Step 3: Jalankan FUSE (Terminal 1)
./mirrorfs /mnt/mirror

#Step 4: Jalankan test (Terminal 2, atau pakai & untuk background)
bash test.sh
```

> **Catatan perbedaan x86 vs ARM64:**
> Tidak ada perubahan apapun pada source code. Perbedaan hanya internal di level compiler dan library yang dihandle otomatis oleh sistem.

| Aspek | x86_64 | ARM64 |
|---|---|---|
| Source code `mirrorfs.c` | Sama | Sama |
| `setup.sh` | Sama | Sama |
| `test.sh` | Sama | Sama |
| Perintah compile | Sama | Sama |
| Output binary | `ELF 64-bit x86-64` | `ELF 64-bit ARM aarch64` |
| Yang berbeda | — | Hanya binary hasil compile |

| Error | Arti | Kapan digunakan di MirrorFS |
|---|---|---|
| `ENOENT` | No such file or directory | File non-.txt atau path tanpa prefix |
| `EROFS` | Read-only file system | Semua percobaan write (touch, rm, mkdir) |
