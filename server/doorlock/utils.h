#pragma once

#include <Arduino.h>

void bubbleSort(int arr[], int n) {
    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - i - 1; ++j) {
            if (arr[j] > arr[j + 1]) {
				int tmp = arr[j + 1];
				arr[j + 1] = arr[j];
				arr[j] = tmp;
            }
        }
    }
}

/**
 * Alternative to `analogRead` that takes five measurements together and returns
 * the median after dropping the highest and lowest readings.
 *
 * We need this because the (analog) feedback pin of the servo motor is unstable
 * and occasionally jumps to some ridiculous value.
 */
int analogReadStable(byte pin) {
  const int LEN = 9;
  int v[LEN];
  for (byte i = 0; i < LEN; i++) v[i] = analogRead(pin);
  bubbleSort(v, LEN);
  // drop lowest & highest, return mean of middle three
  int sum = 0;
  return (v[1] + v[2] + v[3]) / 3;
}
