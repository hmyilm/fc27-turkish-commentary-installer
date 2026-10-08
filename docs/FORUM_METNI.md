# FC27 PS5 Türkçe Spiker — ELF Kurucusu v0.1.1-beta USB + DATA ve Rehber | PPSA34015 v003/v004

Arkadaşlar, PPSA34015 için Türkçe spiker dosyalarını kuran bir PS5 ELF aracı hazırladık. ZIP USB'de veya konsol depolamasında duruyor; kurucu PS5 üzerinde açıyor, bilinen 10 dosyayı CRC32 ve SHA256 ile kontrol ediyor ve mevcut `ampr_emu.index` içindeki ilgili boyut kayıtlarını düzeltiyor. Kurulum sırasında PC'de Python çalıştırmaya veya FTP bağlantısına gerek yok.

Kaynak v01.000.004 kurulumunda çevrimdışı maç ve Türkçe spiker çalıştı. Dosyaları hazırlanan ZIP'ten tekrar kurup konsoldan doğruladık; yeniden kurulumdan sonra da maça girildi. Test ortamı FW 13.20, ShadowMount+ 1.7beta3 ve klasör biçimindeki oyundu.

Native ELF de PS5 üzerinde denendi: 10 dosyanın tam hash kontrolü geçti; kontrollü olarak eksiltilen bir Türkçe dosya ve indeks kaydı ZIP'ten geri kuruldu. Sonuç konsoldan tekrar okunarak doğrulandı. Bilgisayar testinde 10 dosyalık kurulum ve hata sırasında geri alma da kontrol edildi.

Bu konsol testleri 7 Ekim derlemesine ait. v0.1.1-beta USB + DATA güncellemesi oyun klasörünü bulma yöntemini değiştirir; bu güncellemenin 81 klasör arama testi ve tam kurulum/geri alma entegrasyon testi bilgisayarda geçti. PS5 testi henüz yapılmadı. Önceki konsol sonuçları yeni ELF’in test edildiği anlamına gelmez.

Araç v01.000.003 ve v01.000.004'ü kabul ediyor. Dosyaların kaynağı v004; v003 ve farklı backportlar üzerinde oyun içi test henüz yok. Ayrıntılar GitHub'daki **Test ve uyumluluk durumu** sayfasında.

GitHub'da kaynak kod, ELF ve kurulum rehberi var. Oyun ve Türkçe ses varlıkları depoda bulunmuyor; kurucu ayrıca edinilmiş uyumlu ZIP'i kullanıyor. Açılmış veri boyutu yaklaşık 1,50 GB.

**Güncel kurucu paketi:** [FC27_TR_KURUCU_v0.1.1-beta_USB_DATA.zip](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases/download/v0.1.1-beta/FC27_TR_KURUCU_v0.1.1-beta_USB_DATA.zip). Bu küçük ZIP’i açıp içindeki `FC27_TR_KUR.elf` dosyasını kullanın.

**Güncel kaynak paketi:** [FC27_TR_KAYNAK_v0.1.1-beta_USB_DATA.zip](https://github.com/hmyilm/fc27-turkish-commentary-installer/releases/download/v0.1.1-beta/FC27_TR_KAYNAK_v0.1.1-beta_USB_DATA.zip). GitHub’ın otomatik **Source code** dosyaları ilk v0.1.1-beta kodudur; güncel kaynak bu özel ZIP’te ve `main` dalındadır.

**Araç ve rehber:** [GitHub](https://github.com/hmyilm/fc27-turkish-commentary-installer)

**Türkçe veri ZIP'i:** [MediaFire — yaklaşık 1,37 GB](https://www.mediafire.com/file/y81auk6wpsdlbw8/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip/file).

ZIP SHA256: `78d3a21b8accf760dcef0328be29e0d715df2af8080f6b4bbf0bf78e054bf3f4`

**MCPSP konusu:** [FC 27 PS5 Türkçe Spiker Dosyaları ve ELF Kurucusu](https://www.mcpsp.com/threads/fc-27-ps5-turkce-spiker-dosyalari-ve-elf-kurucusu-ppsa34015-v004.95660/). 8 Ekim 2026'da yayımlandı.

Ses ZIP'indeki metinler PC ile manuel kurulum alternatifini anlatır. ELF kurulumu için [güncel kurulum rehberini](KURULUM.md) izleyin; veri ZIP'ini açmayın ve adını değiştirmeyin.

PS5 ana ekranı için PKG başlatıcısı da hazırlanıyor. PS5 testi ve indirme bağlantısı hazır olduğunda ayrıca eklenecek; önceki ELF sürümünün test sonuçları yukarıdadır.

USB yerleşimi:

```text
/pldmgr/FC27_TR_KUR.elf
/FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

Oyunu tamamen kapatıp Payload Manager'dan ELF'i başlatın. İşlem tamamlandıktan sonra oyunda spiker dilini Türkçe seçip çevrimdışı maç deneyin.

## v0.1.1-beta USB + DATA: Oyun klasörü araması

**Daha önce v0.1.1-beta indirenler de `FC27_TR_KURUCU_v0.1.1-beta_USB_DATA.zip` paketini yeniden indirmeli.** Sürüm numarası aynı kaldı; bu güncellemede `/data` genelinde arama eklendi. Önceki dosyalar aynı yayında durduğu için adında `USB_DATA` bulunan paketi seçin. İçindeki ELF’in başlangıç bildiriminde **`USB+DATA`** yazar. 1,37 GB ses ZIP’i değişmedi, tekrar indirmeye gerek yok. Tek başına `FC27_TR_KUR_USB_DATA.elf` indirdiyseniz dosya adını `FC27_TR_KUR.elf` yapın.

Harici SSD’de **`etaHEN` klasörü veya `PPSA34015-app0` adı zorunlu değil.** USB0–USB7 disklerinin kökü ve kökten en fazla dört klasör derinliği otomatik aranır. Örneğin `SSD/FC27`, `SSD/games/FC27`, `SSD/PS5/FC27` veya `SSD/OnionHEN/games/FC27` kullanılabilir. PS5’in `/data` dizini ve altı da en fazla 32 klasör derinliğinde aranır; `/data/FC27` veya `/data/oyunlarim/FC27` gibi başka yerleşimler de desteklenir.

Kurucu klasör içindeki `sce_sys/param.json` ile PPSA34015 v003/v004 kimliğini, `ampr_emu.index` dosyasını ve `Data/Ps5` dizinini kontrol eder. Birden fazla uygun kopya varsa otomatik seçim yapmaz; `install.conf` içindeki `game=` ayarıyla hedefi seçmek gerekir. Tarama sınırına ulaşıldığında da oyunu taşımadan bu ayar kullanılabilir; ayrıntıları [kurulum rehberinde](KURULUM.md).

**exFAT biçimli harici SSD’deki açılmış oyun klasörü desteklenir.** Oyun PS5’in iç depolamasına kurulmuş olmak zorunda değildir. Oyunun tek `.exfat` / `.ffpfsc` görüntü dosyası olması farklıdır; görüntünün içine doğrudan ekleme yapılmaz.

Eski “Klasor oyun ... bulunamadi. Sikistirilmis oyun desteklenmiyor.” mesajı oyunun sıkıştırılmış olduğunu tespit etmiyordu. Eski kurucunun dar arama konumları dışında kalan klasörlerde de aynı mesaj çıkıyordu. ZIP yerleşimi ve ses dosyaları değişmedi; eski ses ZIP’i kullanılabilir.

Ayrıntılı rehber ve PC ile manuel alternatif GitHub’da.

Deneyenler oyun sürümünü, yükleyici/firmware bilgisini ve Türkçe maç sonucunu yazarsa uyumluluk listesini genişletebiliriz. Tam maç ve yeniden açılış sonuçlarını da eklerseniz yararlı olur.
