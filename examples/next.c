#pragma scop
for (int i = 0; i < 16; i++) {
  for (int j = 0; j < 11; j++) {
    if (i < 1)
      S1(i, j);
    else if (i < 3)
      S2(i, j);
    else if (i < 7)
      S3(i, j);
    else if (i < 15)
      S4(i, j);
  }
}
#pragma endscop
