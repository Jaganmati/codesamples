import numpy as np
from PIL import Image
import matplotlib
from matplotlib import pyplot as plt

plt.rcParams['toolbar'] = 'None'

# Setup the plots for displaying the before and after images
fig, (lplot, rplot) = plt.subplots(1, 2)
lplot.set_title('Before')
rplot.set_title('After')

# Load and display the original image
img = Image.open('spinalscan.bmp')
data = np.array(img)
size = data.shape
channels = size[2] if len(size) > 2 else 1
lplot.imshow(img, cmap = ('gray' if channels == 1 else None))

# Calculate the number of blocks
blockSize = 8
blocks = [int(np.ceil(size[0] / blockSize)), int(np.ceil(size[1] / blockSize))]

blockSizeInv = 1.0 / blockSize
alpha0 = np.sqrt(blockSizeInv)
alpha1 = np.sqrt(2.0 * blockSizeInv)
pibs = np.pi * blockSizeInv

# Generate a mask matrix
maskWidth = 4 # The number of 1s in the first row of the mask
mask = np.zeros(shape=(blockSize, blockSize), dtype='uint8')
for i in range(maskWidth):
    for j in range(maskWidth - i):
        mask[i,j] = 1

# Make a matrix for the decompressed image
result = np.zeros(shape=size, dtype='uint8')

def processBlock(bx, by):
    offsetX = bx * blockSize
    width = np.minimum(blockSize, size[0] - offsetX)

    offsetY = by * blockSize
    height = np.minimum(blockSize, size[1] - offsetY)
    
    colors = data[offsetX:offsetX+width,offsetY:offsetY+height]
    coefficients = np.zeros(shape=(blockSize, blockSize, channels))

    # Calculate the discrete cosine transform for each texel
    alpha_k = alpha0
    alpha = blockSizeInv
    for k in range(maskWidth):
        alpha_j = alpha0
        kpibs = k * pibs
        # Only iterate over values known to be in the final masked product
        for j in range(maskWidth - k):
            jpibs = j * pibs
            for x in range(width):
                xcos = np.cos((x + 0.5) * kpibs)
                for y in range(height):
                    # Calculate the coefficient
                    coefficients[k, j] += alpha * colors[x, y] * xcos * np.cos((y + 0.5) * jpibs)
            if j == 0:
                alpha_j = alpha1
                alpha = alpha_k * alpha_j

        if k == 0:
            alpha_k = alpha1
            alpha = alpha_k * alpha0

    # Skipping saving all of the coefficients in a separate matrix
    # due to the immediate decompression step

    target = result[offsetX:offsetX+width,offsetY:offsetY+height]

    # Calculate the inverse discrete cosine transform
    for x in range(width):
        xpibs = (x + 0.5) * pibs
        for y in range(height):
            ypibs = (y + 0.5) * pibs
            alpha_k = alpha0
            color = np.zeros(shape=channels)
            for k in range(maskWidth):
                kcos = alpha_k * np.cos(k * xpibs)
                alpha_j = alpha0
                for j in range(maskWidth - k):
                    # Calculate the color
                    color += alpha_j * coefficients[k,j] * kcos * np.cos(j * ypibs)
                    alpha_j = alpha1
                alpha_k = alpha1
            target[x,y] = np.clip(color, 0, 255).astype('uint8')

# Loop through each block
for by in range(blocks[1]):
    for bx in range(blocks[0]):
        processBlock(bx, by)

rplot.imshow(Image.fromarray(result, 'RGB' if channels == 3 else None), cmap = ('gray' if channels == 1 else None))
plt.show()
