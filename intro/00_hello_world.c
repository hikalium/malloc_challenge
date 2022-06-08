// ← スラッシュを2つ書くと、そこから行の終わりまでがコメントになる
/* ここもコメントになる */
// ↓ これはprintf関数を使うために必要な、コンパイラへの指示
#include <stdio.h>
// ↓ C言語では、プログラムを実行するとmain関数が最初に実行される
int main() {
  // ↓ printf命令は、文字列や数値を表示することができる。
  printf("Hello, world!\n");
  // %なんとか って書くと、引数に指定した値を表示することができる
  // 詳しいマニュアル: https://man7.org/linux/man-pages/man3/printf.3.html
  // %d と書くと、引数が10進数で表示される
  printf("1st arg = %d\n", 123);
  // %d と書くと、引数が10進数で表示される
  printf("1st arg = %d\n", 123);
  return 0;
}
