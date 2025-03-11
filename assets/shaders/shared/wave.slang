module wave;

/** @brief Returns the minimum value of all active lane value's across the wave. */
public inline uint wave_min(const uint value) {
    return WaveActiveMin(value);
}

/** @brief Returns the maximum value of all active lane value's across the wave. */
public inline uint wave_max(const uint value) {
    return WaveActiveMax(value);
}
