module atomic;

/** @brief Increment atomic variable. */
public inline uint atomic_inc(uint* counter) {
    uint output; InterlockedAdd(*counter, 1, output); return output;
}

/** @brief Decrement atomic variable. */
public inline uint atomic_dec(uint* counter) {
    uint output; InterlockedAdd(*counter, -1, output); return output;
}

/** @brief Set atomic variable. */
public inline uint atomic_set(uint* addr, uint value) {
    uint output; InterlockedExchange(*addr, value, output); return output;
}

/** @brief Set atomic variable if the new value is smaller than the current one. */
public inline uint atomic_min(uint* addr, uint value) {
    uint output; InterlockedMin(*addr, value, output); return output;
}

/** @brief Set atomic variable if the new value is greater than the current one. */
public inline uint atomic_max(uint* addr, uint value) {
    uint output; InterlockedMax(*addr, value, output); return output;
}

/** @brief Set the value in `src` and put the previous value of `src` into `dst`. */
public inline void atomic_exchange(uint* src, uint value, uint* dst) {
    InterlockedExchange(*src, value, *dst);
}
