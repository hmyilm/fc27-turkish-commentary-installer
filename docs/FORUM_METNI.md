# FC27 PS5 Türkçe Spiker — Yerel ELF Kurucusu ve Rehber | PPSA34015 v003/v004

Arkadaşlar, PPSA34015 için Türkçe spiker dosyalarını kuran bir PS5 ELF aracı hazırladık. ZIP USB'de veya konsol depolamasında duruyor; kurucu PS5 üzerinde açıyor, bilinen 10 dosyayı CRC32 ve SHA256 ile kontrol ediyor ve mevcut `ampr_emu.index` içindeki ilgili boyut kayıtlarını düzeltiyor. Kurulum sırasında PC'de Python çalıştırmaya veya FTP bağlantısına gerek yok.

Kaynak v01.000.004 kurulumunda çevrimdışı maç ve Türkçe spiker çalıştı. Dosyaları hazırlanan ZIP'ten tekrar kurup konsoldan doğruladık; yeniden kurulumdan sonra da maça girildi. Test ortamı FW 13.20, ShadowMount+ 1.7beta3 ve klasör biçimindeki oyundu.

Native ELF de PS5 üzerinde denendi: 10 dosyanın tam hash kontrolü geçti; kontrollü olarak eksiltilen bir Türkçe dosya ve indeks kaydı ZIP'ten geri kuruldu. Sonuç konsoldan tekrar okunarak doğrulandı. Bilgisayar testinde 10 dosyalık kurulum ve hata sırasında geri alma da kontrol edildi.

Bu konsol testleri 7 Ekim derlemesine ait. 8 Ekim yayın dosyasında kişisel derleme yolu metinleri kaldırıldı; derlenen çalıştırılabilir kod bölümleri aynı, ancak yeni ikili için tekrar konsol testi henüz yapılmadı.

Araç v01.000.003 ve v01.000.004'ü kabul ediyor. Dosyaların kaynağı v004; v003 ve farklı backportlar üzerinde oyun içi test henüz yok. Ayrıntılar GitHub'daki **Test ve uyumluluk durumu** sayfasında.

GitHub'da kaynak kod, ELF ve kurulum rehberi var. Oyun ve Türkçe ses varlıkları depoda bulunmuyor; kurucu ayrıca edinilmiş uyumlu ZIP'i kullanıyor. Açılmış veri boyutu yaklaşık 1,50 GB.

**Araç ve rehber:** [GitHub](https://github.com/hmyilm/fc27-turkish-commentary-installer)

**Türkçe veri ZIP'i:** MediaFire bağlantısı paylaşım öncesinde buraya eklenecek.

PS5 ana ekranı için PKG başlatıcısı da hazırlanıyor. PS5 testi ve indirme bağlantısı hazır olduğunda ayrıca eklenecek; şu an doğrulanan yöntem ELF.

USB yerleşimi:

```text
/pldmgr/FC27_TR_KUR.elf
/FC27_TR/FC27_TURKCE_SPIKER_PPSA34015_v01.000.004.zip
```

Oyunu tamamen kapatıp Payload Manager'dan ELF'i başlatın. İşlem tamamlandıktan sonra oyunda spiker dilini Türkçe seçip çevrimdışı maç deneyin. Hedef oyun yazılabilir klasör biçiminde olmalı; sıkıştırılmış oyun görüntüsüne doğrudan ekleme yapılmıyor. Ayrıntılı rehber ve PC ile manuel alternatif GitHub'da.

Deneyenler oyun sürümünü, yükleyici/firmware bilgisini ve Türkçe maç sonucunu yazarsa uyumluluk listesini genişletebiliriz. Tam maç ve yeniden açılış sonuçlarını da eklerseniz yararlı olur.
