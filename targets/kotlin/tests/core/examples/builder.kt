// Stops the program with a message, as polyir's `fail` does.
private fun fail(message: String): Nothing {
    System.err.println("trap: " + message)
    exitProcess(1)
}

data class Shape(val side: Long, val name: String, val tags: MutableList<String>) {
    fun size(): Size {
        return Size(this.side * this.side)
    }

    companion object {
        fun square(side: Long): Shape {
            return builder().side(side).name("square").build()
        }

        fun builder(): Builder {
            return Builder(null, "shape", mutableListOf())
        }
    }

    data class Size(val area: Long)

    data class Builder(var __side: Long?, var __name: String, var __tags: MutableList<String>) {
        fun side(value: Long): Builder {
            this.__side = value
            return this
        }

        fun name(value: String): Builder {
            this.__name = value
            return this
        }

        fun tags(value: MutableList<String>): Builder {
            this.__tags = value
            return this
        }

        fun build(): Shape {
            if (this.__side == null) {
                fail("Shape.build: side is not set")
            }
            return Shape(this.__side ?: fail("unwrap of an empty optional"), this.__name, this.__tags)
        }
    }
}

fun tagged(b: Shape.Builder): Shape.Builder {
    val tags: MutableList<String> = mutableListOf("red", "big")
    return b.tags(tags)
}

fun main(): Unit {
    val first: Shape = tagged(Shape.builder()).side(2L).build()
    val second: Shape = Shape.square(3L)
    val size: Shape.Size = second.size()
    println("${first.name} ${first.side} ${first.size().area} [${first.tags[0]}, ${first.tags[1]}] ${second.name} ${second.side} ${size.area}")
}
