!Hey Kamu adalah senior developer c yang membuat desktop application windows menggunakan cmake dan compiler visual studio 2022 secara profesional

Aku ingin membuat desktop aplikasi seperti dsx drumb
hal persiapan:
di soundpack sudah ada beberapa preset yang bisa di gunakan untuk sampling serta mapping json nya!
map nya seperti ini : 

dimulai dari 1 sampai M

[
    1 2 3
    Q W E
    A S D 
    Z X C 
    B N M
]

Alternatifkey 
[
    num(/) num(*) num(-)
    num(7) num(8) num(9)
    num(4) num(5) num(6)
    num(1) num(2) num(3)
    num(0) num(.) num(enter)
]


fitur:
- bisa play wav dan async
- bisa ganti per pad dengan semua nama file yang ada tapi harus sesuai dengan preset
- bisa ganti preset dengan tombol arrow atas dan bawah

dev :
- pakai sound library windows! karena ini wav
- pastikan memory terjaga agar tidak crash saat banyak play (async)
- modularisasi profesional. tanpa src folder
- pakai CMKAE untuk build (hanya windows aku tidak ingin build selain platfrom windows)