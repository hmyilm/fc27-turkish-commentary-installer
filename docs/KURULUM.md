# Kurulum rehberi

Bu kurucu, PPSA34015'in Türkçe spiker dosyalarını yerel ZIP'ten oyun klasörüne ekler. Kaynak ses dosyalarının sürümü v01.000.004'tür. Hedefte v01.000.003 ve v01.000.004 kabul edilir; v003'te oyun içi uyumluluk henüz doğrulanmadı.

Bu rehber v0.1.1-beta içindir. Oyun klasörü araması genişletildi; 60 klasör arama testi ve kurulum/geri alma entegrasyon testi bilgisayarda geçti; yeni ELF henüz PS5 üzerinde denenmedi. Önceki ELF’in konsol testleri ile sürümler arasındaki farklar [test durumunda](TEST_DURUMU.md) belirtilir.

## Gerekenler

- ELF başlatabilen PS5 ortamı ve Payload Manager.
- Yazılabilir klasör biçiminde PPSA34015 oyunu; klasöründe `sce_sys/param.json`, `ampr_emu.index` ve `Data/Ps5` bulunmalı. exFAT biçimli harici SSD kullanılabilir; oyun PS5’in iç depolamasında olmak zorunda değildir.
- `FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip` adlı uyumlu Türkçe veri paketi. Ses dosyaları bu GitHub deposunda yer almaz.
- Hedef oyun depolamasında geçici çıkarma için yeterli boş alan. 10 dosyanın açılmış toplamı **1.503.216.738 bayt**; hedefte yaklaşık **2 GB boş alan** ayırmak uygundur. ZIP aynı depolamada tutulacaksa ZIP için de ayrı alan gerekir.

ZIP'i önceden açmanız gerekmez. Kurucu yalnızca derlemeye kayıtlı 10 dosyayı çıkarır; ZIP'teki metin veya yardımcı dosyaları oyun klasörüne kopyalamaz.

## 1. Oyunu kapatın

FC27 simgesi üzerinde Options → **Oyunu Kapat** seçin. PS5 ana ekranına dönmek tek başına oyunu kapatmaz. Kurucu çalışan FC27'yi algılarsa işlemi durdurur; oyunu kendisi kapatmaz.

## 2. Dosyaları USB'ye koyun

USB'deki yerleşim:

```text
/pldmgr/FC27_TR_KUR.elf
/FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

ZIP'in dosya adını değiştirmeyin. Payload Manager'ın USB kökündeki ELF taraması kapalı olabilir; ELF için `/pldmgr` klasörünü kullanın. Kurucu USB0–USB7 bağlantı noktalarındaki ZIP'i arar.

USB yerine PS5 depolamasını kullanıyorsanız eşdeğer yollar:

```text
/data/pldmgr/payloads/FC27_TR/FC27_TR_KUR.elf
/data/FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

Yerel kurulum sırasında FTP bağlantısı veya internetten indirme yapılmaz. Dosyaları bu konumlara bir kez yerleştirmek yeterlidir.

## 3. Hedef oyun klasörünü belirleyin

v0.1.1-beta oyun klasörünü adına göre değil, içeriğine göre tanır. `sce_sys/param.json` içinde oyun kimliği **PPSA34015**, sürüm **01.000.003** veya **01.000.004** olmalı; aynı klasörde `ampr_emu.index` ve `Data/Ps5` bulunmalı. Kurulumdan önce indeksin gerekli Türkçe kayıtları da doğrulanır.

**Harici USB/SSD:** `/mnt/usb0`–`/mnt/usb7` disklerinin kökü ve kökten en fazla dört klasör derinliğindeki dizinler aranır. `etaHEN` veya `PPSA34015-app0` adı zorunlu değildir. Şu yerleşimler örnektir; oyunu bunlardan birine taşımak gerekmez:

```text
SSD/FC27/
SSD/games/FC27/
SSD/PS5/FC27/
SSD/etaHEN/games/PPSA34015-app0/
SSD/OnionHEN/games/FC27/
```

**PS5 iç depolaması:** `/data/etaHEN/games`, `/data/OnionHEN/games`, `/data/games` ve `/data/PS5` dizinleri ile bu köklerin altında en fazla dört klasör derinliği aranır. Tüm `/data` dizini taranmaz.

Arama kökünün kendisi derinlik 0 sayılır; dördüncü klasör seviyesi de kontrol edilir. Bir oyun `sce_sys/param.json` dosyasıyla karşılaşıldığında o oyunun alt klasörleri taranmaz. Sembolik bağlantılar, ayrı bağlanmış alt dosya sistemleri, gizli klasörler, `System Volume Information` ve `$RECYCLE.BIN` atlanır. Her arama kökü için 4.096 dizin ve 32.768 dizin girdisi sınırı vardır. Bu sınır aşılırsa bulunan ilk oyuna işlem yapılmaz; hedefi `game=` ile belirtmeniz istenir.

Birden fazla uygun oyun klasörü bulunursa otomatik seçim yapılmaz. Oyun arama sınırlarından daha derindeyse, başka bir dahili konumdaysa veya birden fazla kopyası varsa aşağıdaki `game=` ayarını kullanın.

Oyun klasörünün içeriği:

```text
FC27/
├── ampr_emu.index
├── sce_sys/param.json
└── Data/Ps5/...
```

### Oyunu taşımadan hedef seçmek

`/data/FC27_TR/install.conf` adlı düz metin dosyasına tam yolu yazın. Bu ayar dosyası PS5’in `/data/FC27_TR` dizininde olmalı; USB’ye konan `install.conf` okunmaz.

```ini
game=/mnt/usb0/oyunlarim/FC27
zip=/mnt/usb0/FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

`game` oyun klasörünün kendisi, `zip` ZIP dosyasının tam yoludur. Örnekteki klasör ve USB bağlantı noktasını kendi konumunuza göre değiştirin. `game=` ile seçilen klasörün adı ve derinliği serbesttir; aynı kimlik, sürüm ve içerik kontrolleri uygulanır. Hedef normal, yazılabilir bir kaynak oyun klasörü olarak `/data` veya `/mnt/usb0`–`/mnt/usb7` altında bulunmalı; ayrı bağlanmış oyun görüntüsü veya sembolik bağlantı olmamalıdır. İhtiyacınız olmayan `zip=` satırını kaldırabilirsiniz.

ZIP arama konumları değişmedi: önce `/data/FC27_TR`, sonra `/data`, ardından USB0–USB7 üzerindeki `/FC27_TR` ve USB kökü kontrol edilir; ilk bulunan uygun konum kullanılır. Belirli bir ZIP’i seçmek için `zip=` kullanın.

Ayar dosyası yoksa otomatik arama ve kurulum modu kullanılır. `mode=install` satırı da açıkça kurulum modunu seçer.

Sadece ön kontrol için aynı dosyaya şu satırı ekleyin:

```ini
mode=check
```

Kontrol modu oyun dosyalarını değiştirmez. `/data/FC27_TR` dizini varsa sonuç günlük dosyasına kaydedilir. Mevcut dosyaların SHA256 değerlerini ve indeks boyutlarını kontrol edip gereken değişiklikleri bildirimle gösterir. Kuruluma geçeceğiniz zaman `mode=check` satırını kaldırın veya `mode=install` yapın.

## 4. ELF'i başlatın

Payload Manager'da listeyi yenileyin ve `FC27_TR_KUR` ELF'ini başlatın. Bildirimler doğrulama ve kurulum aşamalarını gösterir. Büyük ses dosyalarının okunması depolamaya göre zaman alabilir; işlem bitmeden oyunu açmayın.

Başarı bildirimi sonrası oyunu açın. Ses/spiker ayarında **Türkçe** seçip çevrimdışı maça girin. Dosyalar tanınıyorsa ayrıca oyun içinden **İndir** seçmek gerekmez.

Deneme sonucunu bildirirken oyun sürümünü, yükleyici ve firmware sürümünü, maça girilip girilmediğini ve Türkçe spikerin duyulup duyulmadığını yazın. Tam maç ve yeniden açılış sonuçları da uyumluluk listesine yardımcı olur.

## Hata alırsanız

Günlük:

```text
/data/FC27_TR/installer.log
```

Bu dizin varsa ve yazılabiliyorsa kurulum günlüğü tutulur. Sadece USB kullandıysanız günlük tutmak için `/data/FC27_TR` dizinini önceden oluşturabilirsiniz. Kontrol modu da bu dizine günlük yazabilir; oyun dosyalarına yazmaz.

| Durum | Yapılacak işlem |
| --- | --- |
| FC27 çalışıyor | Oyunu tamamen kapatıp kurucuyu tekrar başlatın. |
| Oyun klasörü bulunamadı | Oyunun açılmış klasör biçiminde olduğunu, `sce_sys/param.json`, `ampr_emu.index` ve `Data/Ps5` içeriğini kontrol edin. Gerekirse `game=` ile tam yolu seçin. v0.1.0-beta yalnız belirli klasörleri aradığı için aynı hatayı farklı konumdaki sağlam oyunlarda da veriyordu. |
| ZIP bulunamadı | Dosya adını ve USB'deki `/FC27_TR` konumunu kontrol edin veya `zip=` ile tam yolu belirtin. |
| Arama sınırına ulaşıldı | Diskteki tarama sınırı doldu; `install.conf` ile tam `game=` yolunu belirtin. Kısmi aramadan otomatik hedef seçilmez. |
| Birden fazla oyun bulundu | `install.conf` ile kullanılacak `game=` yolunu belirtin. |
| Kimlik/sürüm uyuşmuyor | Hedef `param.json` dosyasında PPSA34015 ve 01.000.003/01.000.004 bulunmalı. Başka oyun için kullanmayın. |
| Gerekli indeks yolu yok veya boyutu farklı | Bu indeks mevcut veri paketiyle uyumlu değil. Başka kurulumun tüm indeksini kopyalamayın; günlükle birlikte durumu bildirin. |
| ZIP CRC/SHA256 hatası | ZIP'in içeriği eksik, bozuk veya beklenen paketle farklıdır. Kurucu doğrulanmamış dosyaları kurmaz. |
| Boş alan yetersiz | Hedef oyun depolamasında yer açın. |
| `.tr-stage` / `.tr-previous` dosyaları kaldı | Önce günlüğü inceleyin. Özellikle `.tr-previous` dosyasını silmeyin; önceki dosyanın kurtarma kopyası olabilir. Hangi dosyanın asıl hâl olduğundan emin olmadan tekrar kurmayın. |
| Kurulum başarılı, oyun hâlâ İndir istiyor | Dosya kontrolü oyun içi tanımayı tek başına kanıtlamaz. Kullanılan oyun paketi, backport ve yükleyici bilgisini bildirin. |

Eski sürümdeki “Klasor oyun PPSA34015-app0/app bulunamadi. Sikistirilmis oyun desteklenmiyor.” mesajı genel bir açıklamaydı; oyunun sıkıştırılmış olduğunu tespit ettiği anlamına gelmez. ELF ve ZIP’i başka diske taşımak hedef oyunun bulunmasını tek başına sağlamaz.

## Dosyalarda yapılan değişiklik

Kurucu 10 Türkçe veri dosyasının boyutunu ve tam SHA256 değerini kontrol eder. Doğru dosyalar yerinde kalır. Değişecek dosyalar önce aynı klasörde `.tr-stage` geçici dosyalarına çıkarılır; ZIP CRC32 ve SHA256 kontrolü sonrasında asıl konumlarına geçirilir.

Mevcut `ampr_emu.index` içinde yalnız ilgili Türkçe kayıtların **64 bit dosya boyutu alanları** güncellenir. Kayıtlar, dosya yolları, zaman alanları, kayıt sırası ve hash tablosu korunur. Kabul edilen eski boyut ya sıfır ya da bu paketin beklenen boyutudur; farklı bir dolu boyutta işlem durur.

Dosyalar değiştirilirken önceki sürümler geçici `.tr-previous` kopyalarında tutulur. Normal bir kurulum hatasında geri alınırlar; başarılı doğrulama sonrasında temizlenirler. Elektrik kesintisi sırasında otomatik geri alma garantisi yoktur. Kurucu kalmış geçici dosyaları bulursa üzerine yazmaz.

Bu işlem başlangıç dosyasını, backportu, unlock paketini, oyun kütüphanelerini veya kayıtlı oyunları düzenlemeyi gerektirmez. Sorununuz maça girememe gibi genel bir oyun hatasıysa bu kurucu onu giderdiğini iddia etmez.

## Sıkıştırılmış oyun görüntüsü

Kurucu yazılabilir oyun klasörü içindir. **SSD’nin exFAT biçimli olması desteklenir.** Oyunun tek dosya olan `.ffpfsc` veya exFAT oyun görüntüsü biçiminde olması farklıdır; kurucu bu görüntülerin içine doğrudan ekleme yapmaz. Önce açılmış oyun klasörünü güncelleyip Türkçe spikerin çalıştığını doğrulayın; ardından kendi paketleme aracınızla görüntüyü yeniden oluşturabilirsiniz. Kurulum sonrası yeniden paketlenmiş oyun ayrıca doğrulanmadı.

## PKG başlatıcısı

PS5 ana ekranından aynı ELF kurucusunu başlatacak PKG hazırlanıyor. Önceki ELF sürümü PS5 üzerinde test edildi; v0.1.1-beta’nın ve PKG’nin PS5 testleri henüz yapılmadı. PKG kurulumu ve PS5 üzerindeki başlatma testi tamamlanınca bu bölüm güncellenecektir. PKG kendi başına ses ZIP'ini içermez.

## PC ile manuel alternatif

Native ELF yerine `tools/TR_INDEX_DUZELT.py` kullanılabilir. Bu araç **yerel PC dosyaları** üzerinde çalışır ve PS5'e bağlanmaz.

1. Oyunu tamamen kapatın.
2. ZIP'i açın. `PPSA34015-app0/Data` içeriğini yolları koruyarak mevcut oyun klasörünüzdeki `Data` ile birleştirin. Oyun klasörü içinde ikinci bir `PPSA34015-app0` oluşturmayın.
3. Hedef oyun klasöründeki mevcut `ampr_emu.index` ile aynı oyunun `sce_sys/param.json` dosyasını PC'ye alın.
4. Bu iki dosyayı, ZIP'i ve `TR_INDEX_DUZELT.py` dosyasını aynı PC klasörüne koyun. O klasörde terminal açın. Python 3.9 veya yenisiyle çalıştırın:

   ```sh
   python3 TR_INDEX_DUZELT.py --index ampr_emu.index --param param.json --paket FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip --output ampr_emu_TR.index
   ```

   Windows'ta `python3` yerine `python` veya `py` kullanılabilir.

5. Araç ZIP dosyalarını doğrular ve yeni `ampr_emu_TR.index` üretir; orijinal PC indeksini değiştirmez. Hedef oyunun mevcut indeksini gerektiğinde geri dönebileceğiniz bir adla ayırın ve yeni dosyayı oyun klasörüne `ampr_emu.index` adıyla aktarın.
6. Oyunu açıp Türkçe spikerle çevrimdışı maç deneyin.

Başka oyun paketinden alınan tüm indeksi kullanmayın. Düzeltme, kurulumunuzun kendi indeksine uygulanmalıdır.
