// IMPORTANT: The .scop file WAS MODIFIED by calling Clay in this code
#pragma scop
/* Clay
   iss([0], {-1||2});
 */
for (int i = 0; i < 10; i++) {
  if ((i <= 4) || (i >= 6)) {
    S[i] = S[i - 2];
  }
}
#pragma endscop
