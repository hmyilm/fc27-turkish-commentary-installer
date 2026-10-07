# Test ve uyumluluk durumu

Son güncelleme: 7 Ekim 2026. Bu sayfa doğrulanan sonuçları ve henüz denenmeyen koşulları ayrı gösterir.

## Oyun içi sonuç

| Deneme | Sonuç |
| --- | --- |
| Kaynak PPSA34015 v01.000.004, klasör biçimi, FW 13.20, ShadowMount+ 1.7beta3 | Kullanıcı çevrimdışı maça girdiğini ve Türkçe spikerin çalıştığını doğruladı. |
| 10 Türkçe dosyanın yayıma hazırlanan ZIP'ten aynı PS5'e yeniden kurulması | Dosyaların tamamı konsoldan baştan sona okunarak SHA256 ile doğrulandı; kullanıcı yeniden kurulumdan sonra maça girildiğini bildirdi. |
| v01.000.003 | Kimlik/sürüm kontrolünde kabul edilir. Bu sürümde oyun içi uyumluluk henüz denenmedi. |
| Türkçe içermeyen başka bir oyun paketi / backport | Ayrı eklentiyle oyun içi test yapılmadı. |
| Tam maçın bitirilmesi ve oyunu kapatıp yeniden açarak ikinci maç | Kullanıcı tarafından bildirilmedi. Genel kararlılık iddiasında bulunulmaz. |

## Dosya ve indeks kontrolleri

- Veri paketi 10 dosya ve 1.503.216.738 bayt açılmış içerikten oluşur.
- Kaynak ZIP'teki dosyalar CRC32 ve SHA256 ile kontrol edildi.
- Manuel Python indeks aracı önceki indeksin bir kopyasında denendi. Altı Türkçe kaydın sıfır boyutu düzeltildi; değişen 15 baytın tamamı bu kayıtların boyut alanları içindeydi.
- İndeksin dosya yolları, zaman alanları, kayıt sırası ve hash tablosu korundu. Aynı düzeltmenin tekrar uygulanması yeni değişiklik oluşturmadı.
- Yanlış oyun kimliği reddedildi; v01.000.003 kimlik/sürüm kontrolünde kabul edildi.
- Aynı PS5'e yeniden kurulum sırasında eksik boyutlar simüle edildi. Onarılmış indeks, çalışan kaynak indeksle birebir eşleşti.

## Native ELF'in PS5 testi

Test edilen ELF: **191.808 bayt**; SHA256:

```text
c51d10e7c2606fd2877b37871130616b7a794c9d6f2c9bf2ae5ed01c2a381f11
```

FW 13.20, PPSA34015 v01.000.004 ve ShadowMount+ 1.7beta3 ortamında:

- Kontrol modunda mevcut 10 dosyanın tam SHA256 değerleri ve indeks boyutları konsolda doğrulandı.
- Oyun kapalıyken 202.727 baytlık bir Türkçe TOC dosyası ve bu dosyanın indeks boyutu kontrollü olarak eksiltildi.
- Native ELF ZIP girdisini akış hâlinde açtı; CRC32 ve SHA256 kontrollerinden sonra dosyayı ve ilgili tek indeks boyutunu geri kurdu.
- Konsoldan tekrar okunan dosya ve **tüm indeks**, deneme öncesi SHA256 değerleriyle birebir eşleşti.
- Deneme sonrası orijinal dosya metadata bilgileri geri getirildi; geçici kurulum/test dosyası veya kurulum kilidi kalmadı.
- İlk denemede desteklenmeyen `futimens` çağrısı tespit edildi. Son ELF desteklenen `futimes` alternatifiyle bu testi geçti.

Bu PS5 testi mevcut çalışan kuruluma bir dosyanın yeniden kurulmasını doğrular. Başka oyun paketlerinde veya v003'te oyun içi uyumluluğu kanıtlamaz. Sonuç: [native test kaydı](../tests/native-results.txt).

### Kurulumun bilgisayar üzerinde entegrasyon testi

Native kurucu kodunun host derlemesiyle sentetik PPSA34015 v003/v004 oyun klasörleri kullanıldı:

- v003 kontrol modu geçti; oyun dosyaları değiştirilmedi.
- Yanlış oyun kimliği ve uyumsuz dolu Türkçe indeks boyutu değişiklik yapılmadan reddedildi.
- Yedinci commit yeniden adlandırmasından önce yapay hata verildi; geri alma bütün orijinal dosyaları ve tüm indeksi geri getirdi.
- v003 hedefte 10 dosyanın kurulduğu ve tüm SHA256 değerlerinin doğru olduğu kontrol edildi.
- Yalnızca altı 64 bit indeks boyut alanı değişti; diğer tüm baytlar korundu.
- v003 ve v004'te tekrar çalıştırma dosyaları yeniden yazmadı; inode, mtime ve hash değerleri korundu.

Bu sentetik test v003'te oyunun çalıştığını göstermez. Sonuç: [host entegrasyon kaydı](../tests/host-results.txt).

### PKG başlatıcısı

PKG başlatıcısı hazırlanıyor; PS5 üzerinde çalıştırma testi henüz tamamlanmadı. ELF'in geçtiği testler PKG'nin test edildiği anlamına gelmez.

### Native ZIP çekirdeğinin bilgisayar testi

Native kurucuyla aynı C ZIP okuma/doğrulama kodu macOS arm64 üzerinde derlenip gerçek veri ZIP'iyle çalıştırıldı:

- 10 dosyanın tamamı akış hâlinde okundu; **1.503.216.738 bayt** için miniz CRC32 ve derlemeye kayıtlı SHA256 kontrolleri geçti.
- İlerleme sayacı sırası ve her dosyanın son bayt sayısı kontrol edildi.
- Doğru mevcut dosya kabul edildi; bir baytı değişen veya kısaltılan dosya reddedildi.
- Eksik ZIP girdisi, yinelenen gerekli girdi, yanlış boyut ve bozuk CRC içeren test arşivleri reddedildi.
- Tam doğrulama yaklaşık 11 saniye sürdü; bu host testinde azami yerleşik bellek yaklaşık 2 MB oldu. PS5 hızını veya bellek kullanımını bu ölçümden çıkarmayın.

Ham sonuçlar: [tam ZIP testi](../vendor/tests/host-test-results.txt) ve [hata senaryoları](../vendor/tests/negative-test-results.txt). Bu test PS5'e yazma yapmadı; native kurulum işleminin tamamının testi ayrıca gereklidir.

## Sonuç bildirme

Uyumluluk raporunda şunları belirtin:

- Oyun kimliği ve sürümü.
- Kullanılan oyun paketi/backport, firmware ve yükleyici sürümü.
- Kurucu sonucu ve varsa `installer.log` içindeki hata.
- Türkçe seçimi sonrası maça girilip girilmediği ve spikerin duyulup duyulmadığı.
- Denendiyse tam maç ve yeniden açılış sonucu.

Günlük paylaşmadan önce kişisel klasör adlarını veya diğer özel bilgileri çıkarın.
