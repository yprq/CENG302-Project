#!/bin/bash

echo "=========================================="
echo "1. ZORUNLU KOD KONTROLÜ (Anti-Cheat)"
echo "=========================================="

# Kodlarda zorunlu fonksiyonlar kullanılmış mı kontrol edelim
MISSING_KEYWORD=0

check_keyword() {
    if ! grep -q "$1" "$2"; then
        echo "UYARI: $2 icinde '$1' fonksiyonu bulunamadi!"
        MISSING_KEYWORD=1
    fi
}

# Thread kontrolleri
check_keyword "pthread_create" "myData_pipe.c"
check_keyword "pthread_create" "myData_shm.c"

# Pipe kontrolleri
check_keyword "mkfifo" "myData_pipe.c"
check_keyword "pthread_mutex_lock" "myData_pipe.c"

# SHM kontrolleri
check_keyword "shm_open" "myData_shm.c"
check_keyword "mmap" "myData_shm.c"
check_keyword "sem_wait" "myMore_shm.c"

if [ $MISSING_KEYWORD -eq 1 ]; then
    echo "HATA: Gerekli IPC/Thread fonksiyonlarini kullanmamissiniz. Lutfen kodunuzu duzeltin!"
    # Istersen buraya 'exit 1' koyarak testi tamamen durdurabilirsin.
fi
echo "Kod kontrolleri tamamlandi."
echo ""

echo "=========================================="
echo "2. KODLAR DERLENIYOR"
echo "=========================================="
# Hem Pipe hem SHM derleniyor
gcc myData_pipe.c -o myData_pipe -pthread
gcc myMore_pipe.c -o myMore_pipe -pthread
gcc myData_shm.c -o myData_shm -lrt -pthread
gcc myMore_shm.c -o myMore_shm -lrt -pthread

if [ $? -ne 0 ]; then
    echo "HATA: Derleme basarisiz oldu! C kodlarinizi kontrol edin."
    exit 1
fi
echo "Derleme basarili."
echo ""

echo "=========================================="
echo "3. LOG DOSYALARI KONTROL EDILIYOR"
echo "=========================================="
# Yeni senaryoya uygun sunucu log dosyaları aranıyor
if [ ! -f "web_server_logs.txt" ] || [ ! -f "db_server_logs.txt" ]; then
    echo "HATA: 'web_server_logs.txt' ve 'db_server_logs.txt' dosyalari klasorde yok!"
    echo "Lutfen test dosyalarini proje klasorune ekleyip tekrar deneyin."
    exit 1
fi
echo "Dosyalar bulundu."
echo ""

echo "=========================================="
echo "4. TEST BÖLÜMÜ 1: NAMED PIPE (FIFO)"
echo "=========================================="
# Üretici yeni log dosyalarıyla arka planda başlatılıyor
./myData_pipe web_server_logs.txt db_server_logs.txt &
sleep 1

# Sayfalama mantığını (Pagination) test etmek için 3 kere boşluk, sonra q gönderiyoruz.
echo -e " \n \n \nq\n" | ./myMore_pipe

echo ""
echo "=========================================="
echo "5. TEST BÖLÜMÜ 2: POSIX SHARED MEMORY"
echo "=========================================="
./myData_shm web_server_logs.txt db_server_logs.txt &
sleep 1

# Aynı çoklu girdi simülasyonu SHM için de yapılıyor
echo -e " \n \n \nq\n" | ./myMore_shm

echo ""
echo "=========================================="
echo "TÜM OTOMATİK TESTLER TAMAMLANDI."
echo "Not: Sinyal kontrolunu (Ctrl+C Graceful Shutdown) test etmek icin"
echo "programlari iki ayri terminalde manuel calistirip Ctrl+C'ye basiniz."
echo "=========================================="