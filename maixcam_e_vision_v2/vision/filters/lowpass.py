def blend(old_value, new_value, alpha):
    return old_value + (new_value - old_value) * alpha
