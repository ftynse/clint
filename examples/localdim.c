#pragma scop
for (int i = 0; i < N; i += 2)
  for (int j = 0; j < N; j++)
    S1(i, j);

for (int i = 0; i < N; i++)
  for (int j = 0; j < N; j += 2)
    S2(i, j);

for (int i = 0; i < N; i += 2)
  for (int j = 0; j < N; j += 2)
    S3(i, j);

for (int i = 0; i < N; i++)
  for (int j = 0; j < N; j++)
    if ((i + j) % 2 == 0)
      S4(i, j);
#pragma endscop
