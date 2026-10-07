# Kurulum rehberi

Bu kurucu, PPSA34015'in Türkçe spiker dosyalarını yerel ZIP'ten oyun klasörüne ekler. Kaynak ses dosyalarının sürümü v01.000.004'tür. Hedefte v01.000.003 ve v01.000.004 kabul edilir; v003'te oyun içi uyumluluk henüz doğrulanmadı.

## Gerekenler

- ELF başlatabilen PS5 ortamı ve Payload Manager.
- Yazılabilir klasör biçiminde PPSA34015 oyunu; klasöründe `sce_sys/param.json` ve `ampr_emu.index` bulunmalı.
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

Kurucu bilinen oyun dizinlerinde `PPSA34015-app0` ve `PPSA34015-app` klasörlerini arar. Birden fazla uygun kurulum bulursa rastgele seçim yapmaz; açık bir hedef ister.

Otomatik oyun arama konumları `/data/etaHEN/games`, `/data/OnionHEN/games`, `/data/games` ve USB0–USB7 üzerindeki `/etaHEN/games` dizinleridir. ZIP için önce `/data/FC27_TR`, sonra `/data`, ardından USB0–USB7 üzerindeki `/FC27_TR` ve USB kökü kontrol edilir; ilk bulunan uygun konum kullanılır. Belirli bir ZIP'i seçmek için `zip=` kullanın.

Örneğin:

```text
/data/etaHEN/games/PPSA34015-app0
```

Klasör içinde şu dosyalar bulunmalı:

```text
PPSA34015-app0/
├── ampr_emu.index
├── sce_sys/param.json
└── Data/Ps5/...
```

Klasörünüz farklı konumdaysa `/data/FC27_TR/install.conf` adlı düz metin dosyası kullanın:

```ini
game=/data/etaHEN/games/PPSA34015-app0
zip=/mnt/usb0/FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

`game` oyun klasörünün kendisi, `zip` ZIP dosyasının tam yoludur. Örnekteki USB bağlantı noktasını kendi konumunuza göre değiştirin. Bu dosya yoksa otomatik arama ve kurulum modu kullanılır. `mode=install` satırı da açıkça kurulum modunu seçer.

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
| ZIP bulunamadı | Dosya adını ve USB'deki `/FC27_TR` konumunu kontrol edin veya `zip=` ile tam yolu belirtin. |
| Birden fazla oyun bulundu | `install.conf` ile kullanılacak `game=` yolunu belirtin. |
| Kimlik/sürüm uyuşmuyor | Hedef `param.json` dosyasında PPSA34015 ve 01.000.003/01.000.004 bulunmalı. Başka oyun için kullanmayın. |
| Gerekli indeks yolu yok veya boyutu farklı | Bu indeks mevcut veri paketiyle uyumlu değil. Başka kurulumun tüm indeksini kopyalamayın; günlükle birlikte durumu bildirin. |
| ZIP CRC/SHA256 hatası | ZIP'in içeriği eksik, bozuk veya beklenen paketle farklıdır. Kurucu doğrulanmamış dosyaları kurmaz. |
| Boş alan yetersiz | Hedef oyun depolamasında yer açın. |
| `.tr-stage` / `.tr-previous` dosyaları kaldı | Önce günlüğü inceleyin. Özellikle `.tr-previous` dosyasını silmeyin; önceki dosyanın kurtarma kopyası olabilir. Hangi dosyanın asıl hâl olduğundan emin olmadan tekrar kurmayın. |
| Kurulum başarılı, oyun hâlâ İndir istiyor | Dosya kontrolü oyun içi tanımayı tek başına kanıtlamaz. Kullanılan oyun paketi, backport ve yükleyici bilgisini bildirin. |

## Dosyalarda yapılan değişiklik

Kurucu 10 Türkçe veri dosyasının boyutunu ve tam SHA256 değerini kontrol eder. Doğru dosyalar yerinde kalır. Değişecek dosyalar önce aynı klasörde `.tr-stage` geçici dosyalarına çıkarılır; ZIP CRC32 ve SHA256 kontrolü sonrasında asıl konumlarına geçirilir.

Mevcut `ampr_emu.index` içinde yalnız ilgili Türkçe kayıtların **64 bit dosya boyutu alanları** güncellenir. Kayıtlar, dosya yolları, zaman alanları, kayıt sırası ve hash tablosu korunur. Kabul edilen eski boyut ya sıfır ya da bu paketin beklenen boyutudur; farklı bir dolu boyutta işlem durur.

Dosyalar değiştirilirken önceki sürümler geçici `.tr-previous` kopyalarında tutulur. Normal bir kurulum hatasında geri alınırlar; başarılı doğrulama sonrasında temizlenirler. Elektrik kesintisi sırasında otomatik geri alma garantisi yoktur. Kurucu kalmış geçici dosyaları bulursa üzerine yazmaz.

Bu işlem başlangıç dosyasını, backportu, unlock paketini, oyun kütüphanelerini veya kayıtlı oyunları düzenlemeyi gerektirmez. Sorununuz maça girememe gibi genel bir oyun hatasıysa bu kurucu onu giderdiğini iddia etmez.

## Sıkıştırılmış oyun görüntüsü

Kurucu yazılabilir oyun klasörü içindir. `.ffpfsc` veya exFAT oyun görüntüsüne doğrudan ekleme yapmaz. Önce klasör içeriğini güncelleyip Türkçe spikerin çalıştığını doğrulayın; ardından kendi paketleme aracınızla görüntüyü yeniden oluşturabilirsiniz.

## PKG başlatıcısı

PS5 ana ekranından aynı ELF kurucusunu başlatacak PKG hazırlanıyor. Bu rehberdeki doğrulanmış kurulum yöntemi ELF'tir; PKG kurulumu ve PS5 üzerindeki başlatma testi tamamlanınca bu bölüm güncellenecektir. PKG kendi başına ses ZIP'ini içermez.

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
