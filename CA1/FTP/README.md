<div dir="rtl">

# FTP
## پیاده‌سازی پروتکل FTP با MODE B

در این پروژه، دو برنامه‌ی `ftp-server` و `ftp-client` پیاده‌سازی شده‌اند. ارتباط کنترلی و انتقال فایل با استفاده از TCP انجام می‌شود. برای هر عملیات انتقال فایل نیز یک اتصال داده‌ی مستقل در حالت Passive ایجاد می‌شود.

</div>

---

<div dir="rtl">

## ساختار پروژه

اجزای اصلی پروژه به شرح زیر هستند:

- کلاس `TCPSocket` مسئول ایجاد سوکت TCP و ارسال و دریافت کامل داده‌ها است.
- کلاس `FTPParser` فرمان‌های دریافتی را تجزیه می‌کند و نام فایل‌ها را اعتبارسنجی می‌کند.
- کلاس `FTPServer` نشست‌های کاربران را مدیریت می‌کند و فرمان‌های سمت سرور را اجرا می‌کند.
- کلاس `FTPClient` به سرور متصل می‌شود و فرمان‌های واردشده توسط کاربر را اجرا می‌کند.
- فایل `protocol.hpp` شامل ثابت‌ها و کدهای مورد استفاده در پروتکل است.
- فایل‌های `main_server.cpp` و `main_client.cpp` آرگومان‌های ورودی را دریافت می‌کنند و برنامه‌های مربوطه را اجرا می‌کنند.
- فایل `Makefile` فرایند کامپایل و آماده‌سازی محیط آزمایش با Tmux را مدیریت می‌کند.

## پیش‌نیازها و اجرا

برای ساخت و اجرای پروژه به ابزارهای زیر نیاز دارید:

- کامپایلر `g++` با پشتیبانی از استاندارد C++11
- ابزار `make`
- ابزار `tmux`

در مسیر اصلی پروژه، دستور زیر را اجرا کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-bash">make</code></pre>

این دستور پروژه را کامپایل می‌کند و یک نشست Tmux با دو پنل کنار هم باز می‌کند:

- **پنل سمت چپ:** اجرای سرور FTP روی پورت `9000` با پوشه‌ی `server-files` به‌عنوان مسیر اصلی فایل‌ها
- **پنل سمت راست:** اجرای کلاینت FTP و اتصال آن به سرور؛ کلاینت برای دریافت فرمان آماده خواهد بود

### تغییر تنظیمات سرور

برای تغییر پورت سرور یا پوشه‌ی اصلی فایل‌ها، می‌توانید متغیرهای Make را هنگام اجرا مقداردهی کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-bash">make FTP_PORT=9100 FTP_ROOT=./storage</code></pre>

### مدیریت نشست Tmux

برای بستن نشست آزمایش و پایان دادن به برنامه‌های در حال اجرای آن، در یک ترمینال دیگر دستور زیر را اجرا کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-bash">tmux kill-session -t ftp_test</code></pre>

برای جدا شدن موقت از نشست، بدون متوقف کردن برنامه‌ها، کلیدهای `Ctrl+B` و سپس `D` را فشار دهید.

برای بازگشت به نشست فعال نیز از دستور زیر استفاده کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-bash">tmux attach-session -t ftp_test</code></pre>

### پاک‌سازی فایل‌های اجرایی

برای حذف فایل‌های اجرایی ساخته‌شده، دستور زیر را اجرا کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-bash">make clean</code></pre>

## سناریوهای تست

فرمان‌های FTP در سناریوهای زیر باید در پنل سمت راست، یعنی پنل کلاینت، وارد شوند؛ مگر آن‌که صریحاً اجرای دستور در ترمینال معمولی ذکر شده باشد.

### ۱. آماده‌سازی فایل‌های آزمایشی

پیش از اجرای `make`، در پوشه‌ی اصلی پروژه فایل‌های آزمایشی را ایجاد کنید. این فایل‌ها برای بررسی آپلود و دانلود با اندازه‌های متفاوت استفاده می‌شوند:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-bash">dd if=/dev/urandom of=upload.bin bs=1 count=3189 status=none
cp upload.bin upload.original

: &gt; empty.bin

dd if=/dev/urandom of=block1024.bin bs=1024 count=1 status=none
dd if=/dev/urandom of=block2048.bin bs=1024 count=2 status=none

cp empty.bin empty.original
cp block1024.bin block1024.original
cp block2048.bin block2048.original</code></pre>

پس از آماده‌سازی فایل‌ها، دستور `make` را اجرا کنید تا سرور و کلاینت در دو پنل Tmux باز شوند.

### ۲. ورود به سرور و آپلود فایل‌ها

فرمان‌های زیر را به‌ترتیب در پنل کلاینت وارد کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">USER student
PASS secret
MODE B
PASV
STOR upload.bin
PASV
STOR empty.bin
PASV
STOR block1024.bin
PASV
STOR block2048.bin</code></pre>

برای هر آپلود موفق، انتظار می‌رود پاسخ زیر دریافت شود:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">226 Transfer complete</code></pre>

هر عملیات انتقال فایل به یک اتصال داده‌ی Passive مستقل نیاز دارد؛ بنابراین پیش از هر فرمان `STOR` باید یک فرمان `PASV` جدید اجرا شود.

### ۳. مشاهده‌ی فهرست فایل‌های موجود روی سرور

برای دریافت فهرست فایل‌ها و اندازه‌ی آن‌ها، فرمان‌های زیر را وارد کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">PASV
LIST</code></pre>

خروجی باید شامل نام و اندازه‌ی فایل‌های آپلودشده باشد. برای فایل‌های آزمایشی این سناریو، موارد زیر انتظار می‌روند:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">block1024.bin 1024
block2048.bin 2048
empty.bin 0
upload.bin 3189</code></pre>

### ۴. دانلود فایل‌ها و بررسی صحت انتقال

برای دانلود فایل‌ها از سرور، فرمان‌های زیر را در پنل کلاینت اجرا کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">PASV
RETR upload.bin
PASV
RETR empty.bin
PASV
RETR block1024.bin
PASV
RETR block2048.bin</code></pre>

پس از پایان دانلودها، در یک ترمینال معمولی و از پوشه‌ی اصلی پروژه، فایل‌های دریافت‌شده را با نسخه‌های اولیه مقایسه کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-bash">cmp upload.original upload.bin
cmp empty.original empty.bin
cmp block1024.original block1024.bin
cmp block2048.original block2048.bin</code></pre>

اگر دستورهای `cmp` هیچ خروجی‌ای نمایش ندهند، محتوای فایل‌های دانلودشده با نسخه‌های اولیه یکسان است.

این آزمایش انتقال فایل خالی، فایل دقیقاً ۱۰۲۴ بایتی، فایل دقیقاً ۲۰۴۸ بایتی و فایلی با اندازه‌ی چند بلوک را بررسی می‌کند.

### ۵. بررسی مدیریت خطاهای پروتکل

ابتدا مطمئن شوید وارد سرور شده‌اید و حالت `MODE B` فعال است. سپس، بدون اجرای `PASV`، فرمان زیر را وارد کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">LIST</code></pre>

پاسخ مورد انتظار:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">425 CANT_OPEN_DATA_CONNECTION</code></pre>

برای بررسی نام فایل غیرمجاز، فرمان‌های زیر را اجرا کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">PASV
STOR ../outside.bin</code></pre>

پاسخ مورد انتظار:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">553 NAME_NOT_ALLOWED</code></pre>

برای بررسی درخواست فایل ناموجود نیز فرمان‌های زیر را وارد کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">PASV
RETR missing.bin</code></pre>

پاسخ مورد انتظار:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">550 FILE_NOT_FOUND</code></pre>

### ۶. پایان دادن به نشست

برای خروج از کلاینت و پایان دادن به نشست FTP، فرمان زیر را وارد کنید:

<pre dir="ltr" style="direction: ltr; text-align: left;"><code class="language-text">QUIT</code></pre>

## نکات پروتکل

- فرمان‌های `USER`، `PASS`، `MODE B`، `PASV`، `LIST`، `RETR`، `STOR` و `QUIT` پشتیبانی می‌شوند.
- نام فایل فقط می‌تواند شامل حروف انگلیسی، ارقام، نقطه، خط تیره و زیرخط باشد و طول آن نباید از ۶۴ نویسه بیشتر شود.
- هر بلوک در حالت `MODE B` از سه بخش تشکیل می‌شود:
  1. یک بایت **Descriptor**
  2. دو بایت طول داده با ترتیب بایت **Big-endian**
  3. داده‌ی بلوک
- مقدار Descriptor برابر `0x00` نشان‌دهنده‌ی یک بلوک عادی و مقدار `0x40` نشان‌دهنده‌ی پایان فایل (EOF) است.
- اندازه‌ی داده‌ی هر بلوک حداکثر ۱۰۲۴ بایت است.
- فایل خالی نیز با ارسال یک بلوک EOF با طول صفر منتقل می‌شود.
- در عملیات `STOR`، فایل مقصد فقط پس از دریافت کامل فایل جایگزین می‌شود؛ بنابراین انتقال ناقص نباید نسخه‌ی قبلی فایل مقصد را از بین ببرد.
- برای هر عملیات `LIST`، `RETR` یا `STOR` باید پیش از فرمان انتقال، اتصال داده‌ی Passive جدیدی با `PASV` ایجاد شود.

## ابزارهای استفاده‌شده

- **C++11 و TCP Socket API:** پیاده‌سازی ارتباط شبکه، کلاینت و سرور
- **POSIX Threads:** مدیریت اتصال‌های هم‌زمان
- **Makefile:** کامپایل پروژه و اجرای محیط آزمایش
- **Tmux:** نمایش هم‌زمان سرور و کلاینت در دو پنل کنار هم

</div>