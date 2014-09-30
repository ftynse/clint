#pragma scop
for (int i = 0; i < N; i++) {
  for (int j = 0; j < M; j++) {
    a[i][j]++;
    b[i][j]++;
  }
  for (int j = 0; j < L; j++) {
    c[i][j]++;
  }
}

for (int i = 0; i < A; i++) {
  for (int j = 0; j < B; j++) {
    for (int k = 0; k < N; k++) {
      d[i][j][k]++;
    }
  }
  e[i]++;
}
f++;
#pragma endscop
