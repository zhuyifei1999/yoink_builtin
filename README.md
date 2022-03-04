# yoink_builtin

What if, say, you want to disable certain features from core Python?
Suppose you ask freshmen to implement "pow" manually. Nothing stops them
from using builtin pow, until now.

## Installation

Install with pip by:

```
pip install yoink_builtin
```

## Example


```python
import yoink_builtin
import traceback

foo = yoink_builtin.yoink_type_slot(int, 'nb_power')
# Or alternatively,
# foo = yoink_builtin.yoink_function(pow)

yoink_builtin.lockdown()


def unsafe_code():
    print("Because of yoink, you can't use this:")
    try:
        print(pow(1, 1))
    except Exception:
        traceback.print_exc()

    print("\nBecause of lockdown, you can't unyoink:")
    try:
        yoink_builtin.unyoink(foo)
    except Exception:
        traceback.print_exc()

    print("\nAnd you can't even unlockdown:")
    try:
        yoink_builtin.unlockdown()
    except Exception:
        traceback.print_exc()


unsafe_code()

print('\nBut I can:')
yoink_builtin.unlockdown()
yoink_builtin.unyoink(foo)
print(pow(1, 1))

print('ayyy success')
```

Should show something similar to:
```
Because of yoink, you can't use this:
Traceback (most recent call last):
  File "test.py", line 14, in unsafe_code
    print(pow(1, 1))
NotImplementedError: function has been yoinked

Because of lockdown, you can't unyoink:
Traceback (most recent call last):
  File "test.py", line 20, in unsafe_code
    yoink_builtin.unyoink(foo)
RuntimeError: lockdown

And you can't even unlockdown:
Traceback (most recent call last):
  File "test.py", line 26, in unsafe_code
    yoink_builtin.unlockdown()
RuntimeError: bad frame; call unlockdown from same frame as lockdown

But I can:
1
ayyy success
```

Note: This cannot stop people from messing with CPython internals with ctypes
or bytecode constructor and get the 'unyoinking' anyways. Make sure to add audit
hooks and sandbox the whole environment before running potentially unsafe code.

## License

Copyright (C) 2022 YiFei Zhu

The right is granted to copy, use, modify and redistribute this code
according to the rules in what is commonly referred to as an MIT
license.
