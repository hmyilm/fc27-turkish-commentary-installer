# Üçüncü taraf bileşenler

Projenin kendi kaynak kodu GPL-3.0-or-later lisanslıdır. `LICENSE`, GNU GPL v3 metninin resmi kopyasıdır: <https://www.gnu.org/licenses/gpl-3.0.txt>.

## miniz

- Kaynak: [richgel999/miniz](https://github.com/richgel999/miniz).
- Sabitlenen commit: `174573d60290f447c13a2b1b3405de2b96e27d6c` (miniz 3.1.0).
- Lisans: MIT; tam bildirim [vendor/miniz/LICENSE](vendor/miniz/LICENSE) dosyasında.
- Kod ZIP okuma/çözme ve CRC32 doğrulaması için kullanılır. Upstream kaynakları değiştirilmedi; yerel derleme makroları ve export başlığı [vendor/LOCAL_CHANGES.txt](vendor/LOCAL_CHANGES.txt) içinde belirtilir.

## SHA256 — Brad Conte

- Kaynak: [B-Con/crypto-algorithms](https://github.com/B-Con/crypto-algorithms).
- Sabitlenen commit: `cfbde48414baacf51fc7c74f275190881f037d32`.
- Upstream yazarı kodu kamu malı olarak yayımlamıştır. Bildirim [vendor/sha256/LICENSE.txt](vendor/sha256/LICENSE.txt) dosyasında; özgün açıklama [vendor/sha256/README.md](vendor/sha256/README.md) içinde bulunur.
- Yerel taşınabilirlik ve tanımsız davranış düzeltmeleri [vendor/LOCAL_CHANGES.txt](vendor/LOCAL_CHANGES.txt) içinde açıklanır.

İndirilen upstream dosyalarının kaynak URL'leri ve SHA256 değerleri [vendor/UPSTREAM_SOURCES.txt](vendor/UPSTREAM_SOURCES.txt) içinde tutulur.

## PS5 payload SDK

- Derleme bağımlılığı: [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk), v0.43.
- SDK'nın FreeBSD'den alınan başlıkları BSD lisanslıdır; diğer SDK kodu GPL v3 veya sonrası lisanslıdır. Lisans açıklaması SDK'nın resmi README dosyasındadır.
- SDK bu depoya dahil edilmez; ELF derlemek için ayrıca kurulmalıdır. Dağıtılan ELF için karşılık gelen proje kaynak kodu bu depoda sağlanır.

## Oyun ve ses dosyaları

FC27, PPSA34015 ve oyun içindeki ses/veri varlıklarının hakları ilgili hak sahiplerine aittir. Bu depo bu varlıkları içermez. Projenin açık kaynak lisansı, bu varlıkların dağıtımına veya kullanımına ayrı bir izin sağlamaz.

## PKG oluşturma aracı — LibProsperoPKG

- Kaynak: [SvenGDK/LibProsperoPKG](https://github.com/SvenGDK/LibProsperoPKG/tree/748eabf1b7d17819528cabf367d8e27109d8fce3).
- Sabitlenen commit: `748eabf1b7d17819528cabf367d8e27109d8fce3`.
- Lisans: GPL-3.0-or-later; upstream `LICENSE` ve `NOTICE` dosyalarına bakın.
- Bu C# kitaplığı PKG derleme bağımlılığıdır. .NET kitaplığı ve .NET çalışma zamanı PS5 PKG'sine dahil edilmez. Araç tarafından oluşturulan yardımcı PlayGo dosyaları LibProsperoPKG kaynaklıdır.
- Upstream araç ayrıca gömülü `right.sprx` modülü sağlar. Bu modülün ayrı kaynak/lisans bilgisi upstream bildirimlerinde belgelenmediği için projenin kendi GPL kaynak kodu olarak tanımlanmaz. PKG yayın hazırlığı bu yardımcı modülün incelenmesini de kapsar; bu depoda şu an doğrulanan dağıtım ELF'tir.
- Derleme için kullanılan resmi .NET SDK da PS5 paketine eklenmez. Başlatıcı köprüsü, GPL-3.0-or-later lisanslı John Törnblom PS5 payload SDK örneğinden uyarlanır.

PKG'nin PS5 başlatma testi, ELF kurucusunun testinden ayrı tutulur.
