// CENG302_AUTO_EVAL_READY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>

// Paylaşılan bellek ve semafor isimleri (myData_shm ile aynı olmalı)
#define SHM_SIZE 2048
#define SHM_NAME "/my_shm"
#define SEM_EMPTY "/sem_empty"
#define SEM_FULL "/sem_full"

void *shm_ptr;
sem_t *sem_empty, *sem_full;

// Sinyal Yakalayıcı: Ctrl+C basıldığında sistem kaynaklarını temizler
void cleanup_shm(int sig) {
    printf("\n[INFO] SIGINT alindi. Kaynaklar temizleniyor...\n");
    
    // Bellek bağlantısını kes ve sil
    munmap(shm_ptr, SHM_SIZE);
    shm_unlink(SHM_NAME);
    
    // Semaforları kapat ve sil
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    
    exit(0);
}

int main() {
    // Sinyal yakalayıcıyı kaydet
    signal(SIGINT, cleanup_shm);

    // 1. Paylaşılan bellek alanına bağlan
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("[HATA] Paylasilan bellek acilamadi. Once myData_shm calismali");
        return 1;
    }
    shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // 2. Mevcut semaforlara bağlan
    sem_empty = sem_open(SEM_EMPTY, 0);
    sem_full = sem_open(SEM_FULL, 0);

    int shown_count = 0;
    int iter_idx = 0; // Hocanın istediği zorunlu değişken ismi

    printf("[BASLADI] Shared Memory baglantisi kuruldu. Loglar bekleniyor...\n\n");

    while (1) {
        // Masada veri olana kadar bekle (Trafik ışığı: FULL)
        sem_wait(sem_full);

        char line[1024];
        strncpy(line, (char*)shm_ptr, 1024);

        // myData tarafı bitince gönderilen özel çıkış komutu
        if (strcmp(line, "EXIT") == 0) {
            break;
        }

        // --- ADIM 1: FİLTRELEME (QA Görevi) ---
        if (strstr(line, "ERROR") != NULL || strstr(line, "CRITICAL") != NULL) {
            printf("%s", line);
            shown_count++;
            iter_idx++; // Her başarılı işlemde artırıyoruz

            // --- ADIM 2: SAYFALAMA (Pagination) ---
            if (shown_count % 10 == 0) {
                printf("\n--- [%d satir gosterildi. Devam: ENTER | Cikis: q + ENTER] ---\n", shown_count);
                
                char input[10];
                // stdin temizliği ve güvenli okuma
                if (fgets(input, sizeof(input), stdin)) {
                    if (input[0] == 'q' || input[0] == 'Q') {
                        printf("[INFO] Kullanici istegiyle cikiliyor...\n");
                        cleanup_shm(0); // Direkt temizlik fonksiyonuna git
                    }
                }
            }
        }

        // Veriyi okuduk, masayı boşalttık sinyali ver (Trafik ışığı: EMPTY)
        sem_post(sem_empty);
    }

    printf("\n[BITTI] Islem tamamlandi. Toplam %d kritik log gosterildi.\n", shown_count);
    
    // Normal kapanış temizliği
    cleanup_shm(0);

    return 0;
}