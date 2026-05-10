# tixy

A 16×16 grid of dots controlled by a single JavaScript expression.

## Parameters

| param | meaning |
|-------|---------|
| `t` | time in seconds (float, increases each frame) |
| `i` | dot index, 0–255 (left-to-right, top-to-bottom) |
| `x` | column, 0–15 |
| `y` | row, 0–15 |

## Return value

The expression's return value controls each dot:

- **0** → invisible
- **positive** → white dot, size proportional to value
- **negative** → red dot, size proportional to absolute value
- **±1** → full-size dot
- values beyond ±1 are clamped to full size

`Math` is available without the prefix — `sin`, `cos`, `sqrt`, `PI`, etc.

## Examples

### Basics

```js
Math.random() < 0.1    // ~10% of dots visible each frame
Math.random()          // random size per dot per frame
Math.sin(t)            // all dots pulse together over time
i / 256                // brightness gradient by index
x / 16                 // gradient left→right
y / 16                 // gradient top→bottom
y - 7.5               // white top half, red bottom half
```

### Animation

```js
y - t                  // wipe downward
y - t*4                // wipe faster
```

### Patterns

```js
[1, 0, -1][i%3]        // repeating white / off / red columns
y - x                  // diagonal split
(y > x) && (14-x < y) // quarter triangle
i%4 - y%4             // diagonal stripes
x%4 && y%4            // dot grid
x>3 & y>3 & x<12 & y<12                    // square
x==0 | x==15 | y==0 | y==15               // frame
y == x || -(15-x == y)                     // X diagonals
(y-6) * (x-6)                             // mondrian squares
4 * t & i & x & y                         // sierpinski
(t*10) & (1<<x) && y==8                   // binary clock
```

### Animated patterns

```js
-(x>t & y>t & x<15-t & y<15-t)           // shrinking square
(y-4*t|0) * (x-2-t|0)                    // moving cross
(x-y) - sin(t) * 16                      // wipe
(x-y)/24 - sin(t)                        // soft wipe
sin(t*5) * tan(t*7)                       // disco
```

### Math

```js
sin(t-sqrt((x-7.5)**2+(y-6)**2))          // ripple from center
sin(x/2) - sin(x-t) - y+6               // waves
sin(2*atan((y-7.5)/(x-7.5))+5*t)        // rotation
8*t%13 - hypot(x-7.5, y-7.5)            // expanding rings
```

### Noise

```js
random() * 2 - 1       // random noise (changes every frame)
sin(i ** 2)            // static smooth noise
cos(t + i + x * y)    // animated smooth noise
```

### Community

```js
// bloop bloop bloop — @v21
(x-8)*(y-8) - sin(t)*64

// fireworks — @p_malin and @aemkei
-.4/(hypot(x-t%10,y-t%8)-t%2*9)

// ripples — @thespite
Math.sin(t-Math.sqrt(x*x+y*y))

// scrolling TIXY font — @atesgoral
[5463,2194,2386][y+t*9&7]&1<<x-1

// 3d checker board — @p_malin
(((x-8)/y+t*5)&1^1/y*8&1)*y/5

// sticky blood — @joeytwiddle
y-t*3+9+3*cos(x*3-t)-5*sin(x*7)

// 3d starfield — @p_malin
d=y*y%5.9+1,!((x+t*50/d)&15)/d

// dialogue with an alien — @chiptune
1/32*tan(t/64*x*tan(i-x))

// space invader — @keithclarkcouk + @zozuar
'p}¶¼<¼¶}p'.charCodeAt(x)&2**y

// hungry pac man — @p_malin and @aemkei
hypot(x-=t%4*5,y-=8)<6&&x<y|y<-x

// spectrum analyser — @joeytwiddle
x&y<9&y>4+sin(8*t+x*x)+x/4
```
