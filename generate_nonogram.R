library(imager)
library(jsonlite)


ToSegments <- function(slice) {
    d <- subset(as.data.frame(slice), cc == 4)
    r <- rle(d$value>.5)
    paste(r$lengths[r$values], collapse = " ")
}

ProcessFile <- function(filename) {
    i <- load.image(filename)
    rows <- idply(i, "y", ToSegments)$V1
    cols <- idply(i, "x", ToSegments)$V1
    j <- toJSON(list(rows = rows, cols = cols))
    cat(j, file= sub("\\.png", ".json", filename))
}

for (f in list.files(pattern = "*.png")) {
    ProcessFile(f)
}
