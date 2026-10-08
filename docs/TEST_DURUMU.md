# Test ve uyumluluk durumu

Son güncelleme: 8 Ekim 2026. Bu sayfa doğrulanan sonuçları ve henüz denenmeyen koşulları ayrı gösterir.

## v0.2.0-beta — görüntü katmanı

ELF: **227.256 bayt**, SHA256 `63cff8e096e70ae903f888616d603fc872d5cbb18854119c30bfdb3f92de4208`. Bu sürüm ayrı bir beta yayınıdır; aşağıdaki v0.1.1 geçmişini değiştirmez.

8 Ekim 2026, FW 13.20, ShadowMount+ 1.7beta3, PPSA34015 v01.000.004:

- Kayıtlı kaynak 92.201.680.896 bayt `.ffpfsc`; içinde exFAT oyun görüntüsü bulunuyor. İç görüntüde 67.108.864 bayt boş alan vardı.
- Kontrol modu yeni hedef klasörü oluşturmadı; oyun/görüntü dosyalarına yazmadı.
- İlk denemelerde ShadowMount’un 10 saniyelik klasör kararlılık beklemesi ve unionfs’nin üst diskin yazılabilir bayraklarını bildirmesi saptandı. Kod bu gerçek davranışlara göre düzeltildi. Temel görüntünün salt okunur olması zorunlu kaldı.
- ELF 10 Türkçe dosyayı ve oyunun kendi indeksini `/data/homebrew/backports/PPSA34015` altına kurdu. ZIP CRC/SHA256, hedef dosyaların tam geri okuması ve oyunun göreceği yollardan tam doğrulama geçti.
- Ayrı bir yeniden bağlama denemesinde yalnız üst katmana konulan geçici işaret dosyası oyun görünümünden okundu; sonra silindi. Böylece görüntü içindeki mevcut dosyalara düşülerek yanlış doğrulama yapılmadığı ayrıca kontrol edildi.
- Kullanıcı yeni kurulumdan sonra **Türkçe spikerle çevrimdışı maça girdiğini doğruladı**.
- Sonraki kurulum denemesi oyun açık olduğu için değişiklik yapılmadan engellendi. Konsolda tekrar kurulum/no-op testi tamamlanmadı; bu davranış bilgisayarda doğrulandı.
- Görüntü dosyasına yazan işlem yoktur; kaynak kimliği/boyutu/zamanı işlem boyunca kontrol edildi. 92 GB görüntünün tamamı için yeniden SHA256 hesaplanmadı.

**Sınırlar:** Kaynak görüntü zaten çalışan Türkçe dosyalarını içeriyordu; ayrı katmanın kurulması ve okunması doğrulandı. Bu sonuç Türkçesiz başka bir dağıtımın, Pippo paketinin, v003’ün, FW 13.60’ın veya tek başına exFAT görüntüsünün oyun içi doğrulandığı anlamına gelmez. Tam maç, konsol yeniden başlatma ve uzun süreli test henüz yok.

Bilgisayar testleri:

- 81 klasör arama senaryosu.
- 55 sentetik kurulum/geri alma senaryosu: kaynak değişmeden kurulum, başka backport dosyalarının korunması, eksik hedefte yazmayan kontrol modu, uyumsuz indeks/AMPR paketi reddi, mevcut hedefte 22 ve yeni hedefte 11 yeniden adlandırma hata noktası, iptal ve son dosya doğrulaması.
- ShadowMount modülü: 442 doğrulama, ASan + UBSan; API/JSON/HTTP, kaynak ve bağlama kimliği, seçilen hedef, kararlılık beklemesi ve temizleme.
- Gerçek 1,37 GB ZIP ile 10 dosyanın kurulumu, altı indeks boyutunun düzeltilmesi, hata sonrası geri alma, v003/v004 kimlik kabulü ve tekrar kurulumda değişiklik olmaması.

Çalıştırma: `python3 tests/test_discovery.py`, `python3 tests/test_overlay.py`, `python3 tests/test_shadowmount.py`. Gerçek veri testi özel ZIP ve indeks gerektirir; varlıklar depoya eklenmez.

Aşağıdaki bölümler **önceki sürümlerin tarihî kayıtlarıdır**.

## v0.1.1-beta USB + DATA güncellemesi

Sürüm numarası aynı kalan bu yeni ELF, ilk v0.1.1-beta dosyasından farklıdır. Dahili arama artık belirli dört oyun diziniyle sınırlı değildir; `/data` kökünden en fazla 32 klasör derinliği taranır. USB0–USB7 köklerinde dört klasör derinliği korunur. Daha önce v0.1.1-beta indirildiyse güncel `FC27_TR_KURUCU_v0.1.1-beta_USB_DATA.zip` paketi yeniden indirilmelidir; ses ZIP’i değişmedi. Güncel ELF’in başlangıç bildiriminde `USB+DATA` yazar. Aynı yayında duran eski araçlar ve otomatik GitHub kaynak arşivleri bu güncellemeyi içermez; güncel kaynak özel `FC27_TR_KAYNAK_v0.1.1-beta_USB_DATA.zip` paketinde ve `main` dalındadır.

Klasörün adı yerine PPSA34015 v003/v004 kimliği ve gerekli oyun yapısı kontrol edilir. Birden fazla hedef veya arama sınırının aşılması durumunda otomatik seçim yapılmaz; `install.conf` istenir. Sembolik bağlantılar ve ayrı bağlanmış alt dosya sistemleri izlenmez.

- USB + DATA güncellemesinin bilgisayar üzerinde klasör arama testleri: **81 senaryo geçti**. `/data` kökünden gerçek otomatik arama akışı da geçici test dizinleriyle doğrulandı. [Sonuçlar](../tests/discovery-host-results.txt); tekrar çalıştırmak için `python3 tests/test_discovery.py`.
- Güncel host derlemesiyle 10 dosyalık tam kurulum, yanlış kimlik/boyut reddi, yapay hata sonrası geri alma ve tekrar çalıştırmanın dosyaları değiştirmemesi **yeniden doğrulandı**. [USB + DATA entegrasyon sonucu](../tests/host-v0.1.1-results.txt).
- Güncel ELF: **192.176 bayt**, SHA256: `a553f01fc79bf8b9b14ef5eb7179c6c9d4fd60aa0fc6427cb6e3ba97e5bbfae7`.
- Güncel ELF’in PS5 üzerinde çalıştırılması: **henüz yapılmadı**.
- Aşağıdaki eski konsol ve kurulum sonuçları geçmiş derlemelere aittir; güncel ELF için yeni bir konsol doğrulaması olarak değerlendirilmemelidir.
- Ses ZIP’i, beklenen 10 veri dosyası ve bunların hash değerleri değişmedi.

### İlk v0.1.1-beta derlemesi (USB araması ve belirli dahili kökler)

Bu önceki dosya USB’de klasör adı sınırını kaldırdı; dahili arama `/data/etaHEN/games`, `/data/OnionHEN/games`, `/data/games` ve `/data/PS5` ile sınırlıydı. Bu sınırlama yukarıdaki USB + DATA güncellemesinde kaldırıldı.

- Önceki derlemenin 60 klasör arama senaryosu bilgisayarda geçti. Yeni `/data` genelini arama davranışı bu 60 sonucun kapsamına dahil değildir.
- Önceki host derlemesiyle 10 dosyalık tam kurulum, yanlış kimlik/boyut reddi, yapay hata sonrası geri alma ve tekrar çalıştırmanın dosyaları değiştirmemesi doğrulandı.
- Önceki ELF: **192.176 bayt**, SHA256: `15c7a0bdc3ba784a0d1e700852dc22b0cd43c835faaa5df17cbe35c6fb3c1eb0`.
- Bu önceki ELF’in PS5 testi de yapılmadı.

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

7 Ekim'de konsolda test edilen ELF: **191.808 bayt**; SHA256:

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

### 8 Ekim v0.1.0-beta yayın derlemesi

Yayın dosyasında geliştiricinin yerel klasör yolu kalmaması için derleme betiğine `-ffile-prefix-map` eklendi. Kurucu kaynak kodu değişmedi. Yeni ELF **191.808 bayt**, SHA256:

```text
68afa145186704f44065684a7ab7722f0f1e3269db0a1064fd00afabcb73d048
```

Yedi nesne dosyasının tüm çalıştırılabilir bölümleri önceki derlemeyle bayt bayt aynı. Beş nesne dosyası bütünüyle aynı; kalan ikisinde yalnız miniz'in iki assert kaynak yolu metni ve bu metinlere ait sembol boyut/konum bilgileri değişti. Bağlama sırasında yeni metin konumlarına göre adresler yeniden hesaplandı. İkili dosyanın SHA256 değeri bu nedenle değişti.

Yeni yayın derlemesi konsolda henüz yeniden çalıştırılmadı; yukarıdaki 7 Ekim PS5 testleri eski SHA256'ya aittir. [Derleme karşılaştırma kaydı](../tests/build-normalization-results.txt).

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
