// A program in "tiny", a small language whose compiler (tinyc.py) targets polyir.
fn square(x) {
  return x * x
}

fn sumsq(n) {
  let total = 0
  let i = 1
  while i <= n {
    total = total + square(i)
    i = i + 1
  }
  return total
}

print sumsq(4)
print sumsq(10)
