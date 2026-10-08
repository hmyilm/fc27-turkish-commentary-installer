# FC27 PS5 Türkçe Spiker — ELF v0.2.0-beta | Klasör ve Sıkıştırılmış Oyun Desteği

Türkçe spiker kurucusuna oyun görüntüsü desteği eklendi. **Oyunu Game Compressor ile açıp yeniden sıkıştırmaya gerek kalmadan**, ShadowMount’un ayrı dosya katmanına kurulum yapabiliyor.

**Test:** FW 13.20, ShadowMount+ 1.7beta3, PPSA34015 v004, FFPFSC içindeki exFAT görüntüsü. Yeni ELF ile 10 Türkçe dosya kuruldu; CRC/SHA256 ve oyunun gördüğü dosyalar doğrulandı. Yeniden bağlama sonrasında Türkçe spikerle maça girildi.

Açılmış oyun klasörleri de destekleniyor. Harici SSD/USB kullanılabilir; `etaHEN` klasörü veya belirli oyun klasörü adı zorunlu değil. `/data` altında ve USB’de otomatik arama var. Birden fazla kopya varsa rehberdeki `game=` ayarıyla oynadığınız kaynağı seçin.

**Kurucu:** [FC27_TR_KURUCU_v0.2.0-beta.zip](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases/download/v0.2.0-beta/FC27_TR_KURUCU_v0.2.0-beta.zip)

**Kaynak ve rehber:** [GitHub](https://github.com/hmyilm/fc27-turkish-commentary-installer)

**Ses ZIP’i:** [MediaFire — 1,37 GB](https://www.mediafire.com/file/y81auk6wpsdlbw8/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip/file). Önceki ses ZIP’i aynı; tekrar indirmeye gerek yok.

Küçük kurucu ZIP’ini açın. Büyük ses ZIP’ini açmayın. USB yerleşimi:

```text
/pldmgr/FC27_TR_KUR.elf
/FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

FC27’yi tamamen kapatın, görüntü kullanıyorsanız ShadowMount açık olsun. Payload Manager’dan ELF’i başlatıp tamamlandı bildirimini bekleyin. Oyunda Türkçe spikeri seçin.

Görüntüye yazılmaz; sesler mevcut seçili backport klasörüne, yoksa `/data/homebrew/backports/PPSA34015` altına eklenir. Diğer backport dosyaları korunur. Bu klasör daha sonra silinmemeli.

**Sınırlar:** v003 kabul edilir ancak oyun içi denenmedi. FW 13.60, tek başına exFAT ve farklı oyun/backport paketleri ayrıca test edilmedi. `ampr_assets.index` içeren AMPR varlık paketleri henüz desteklenmiyor. Bu araç mevcut maça girememe veya bozuk oyun sorunlarını kesin çözer diye sunulmuyor. Tam maç ve konsol yeniden başlatma testleri henüz bildirilmedi.

Ses ZIP’i SHA256: `78d3a21b8accf760dcef0328be29e0d715df2af8080f6b4bbf0bf78e054bf3f4`

Deneyenler oyun sürümü, firmware, yükleyici ve Türkçe maç sonucunu yazarsa uyumluluk tablosunu gerçek sonuçlarla genişletebiliriz.
