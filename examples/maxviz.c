#pragma scop
for (i1 = 0; i1 < N1; i1++) {
  for (j11 = 0; j11 < N2; j11++) {
    S1(i1, j11);
    S2(i1, j11);
    S3(i1, j11);
  }
  S4(i1);
  S5(i1);
  for (j12 = 0; j12 < N3; j12++) {
    S6(i1, j12);
  }
  for (j13 = 0; j13 < N4; j13++) {
    S7(i1, j13);
    S8(i1, j13);
  }
}
S9;
S10;
for (i2 = 0; i2 < N5; i2++) {
  for (j21 = 0; j21 < N6; j21++) {
    S11(i2, j21);
  }
}
for (i3 = 0; i3 < N7; i3++) {
  for (j31 = 0; j31 < N8; j31++) {
    S12(i3, j31);
    S13(i3, j31);
  }
  for (j32 = 0; j32 < N9; j32++) {
    S14(i3, j32);
  }
}
for (i4 = 0; i4 < N10; i++) {
  S15(i4);
  S16(i4);
}
#pragma endscop
