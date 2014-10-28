int main() {
#pragma scop
// |\   /|
// | ) ( |
// |/   \|
for (int i = 0; i < 10; i++) {
  for (int j = i; j <= 10 - i; j++) {
    printf("%d, %d\n", i, j);
  }
  for (int j = 10 - i; j < i; j++) {
    printf("%d, %d\n", i, j);
  }
}

//   ^   _____
//  / \  \   /
// /___\  \ /
//         v
for (int i = 0; i <= 6; i++) {
  for (int j = 0; j <= 6; j++) {
    if (((i <= 3) && (j <= i)) || ((i > 3) && (j <= 6-i))) {
      printf("%d, %d\n", i, j);
    }
  }

  for (int j = 0; j <= 6; j++) {
    if (((i <= 3) && (j >= 6 - i)) || ((i > 3) && (j >= i))) {
      printf("%d, %d\n", i, j);
    }
  }
}

for (int i = 0; i <= 6; i++) {
  for (int j = 0; j <= i; j++) {
    printf("%d, %d\n", i, j);
  }
  for (int j = i; j <= 6; j++) {
    printf("%d, %d\n", i, j);
  }
}

for (int i = 0; i <= 6; i++) {
  for (int j = 0; j <= 6 - i; j++) {
    printf("%d, %d\n", i, j);
  }
  for (int j = 6 - i; j <= 6; j++) {
    printf("%d, %d\n", i, j);
  }
}

#pragma endscop
}
