[Skip to content](https://github.com/pedropro/TACO#start-of-content)

You signed in with another tab or window. [Reload](https://github.com/pedropro/TACO) to refresh your session.You signed out in another tab or window. [Reload](https://github.com/pedropro/TACO) to refresh your session.You switched accounts on another tab or window. [Reload](https://github.com/pedropro/TACO) to refresh your session.Dismiss alert

{{ message }}

master

[**3** Branches](https://github.com/pedropro/TACO/branches) [**2** Tags](https://github.com/pedropro/TACO/tags)

[Go to Branches page](https://github.com/pedropro/TACO/branches)[Go to Tags page](https://github.com/pedropro/TACO/tags)

Go to file

Code

Open more actions menu

## Latest commit

![author](https://github.githubassets.com/images/gravatars/gravatar-user-420.png?size=40)

pedro

[Merge branch 'cloudflare-migration'](https://github.com/pedropro/TACO/commit/70479ad9adc3da971a8ae75ac97eb1026910eec7)

4 days agoOct 20, 2026

[70479ad](https://github.com/pedropro/TACO/commit/70479ad9adc3da971a8ae75ac97eb1026910eec7) · 4 days agoOct 20, 2026

## History

[196 Commits](https://github.com/pedropro/TACO/commits/master/)

Open commit details

[View commit history for this file.](https://github.com/pedropro/TACO/commits/master/) 196 Commits

## Folders and files

| Name | Name | Last commit message | Last commit date |
| --- | --- | --- | --- |
| [data](https://github.com/pedropro/TACO/tree/master/data "data") | [data](https://github.com/pedropro/TACO/tree/master/data "data") | [Serve images from Cloudflare R2 instead of Flickr](https://github.com/pedropro/TACO/commit/3844bcd3535cd12ec0218494d1d79f1372f553cc "Serve images from Cloudflare R2 instead of Flickr  All 3,433 Flickr-hosted images are now at img.tacodataset.org: originals at /img/<flickr_id>.jpg and 640px versions at /thumb/<flickr_id>_640.jpg. Only the URLs change; OpenLitterMap URLs are unchanged. data/url_map.csv maps old to new URLs.  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>") | 4 days agoOct 20, 2026 |
| [detector](https://github.com/pedropro/TACO/tree/master/detector "detector") | [detector](https://github.com/pedropro/TACO/tree/master/detector "detector") | [Adding more features](https://github.com/pedropro/TACO/commit/090189437289230693649edbd07c11d80023f5ac "Adding more features") | 6 years agoMar 5, 2020 |
| [.gitignore](https://github.com/pedropro/TACO/blob/master/.gitignore ".gitignore") | [.gitignore](https://github.com/pedropro/TACO/blob/master/.gitignore ".gitignore") | [Weekly update](https://github.com/pedropro/TACO/commit/7ca58c9e43eeb8ba0df75a84b789aba4f1dab491 "Weekly update") | 6 years agoDec 28, 2020 |
| [LICENSE](https://github.com/pedropro/TACO/blob/master/LICENSE "LICENSE") | [LICENSE](https://github.com/pedropro/TACO/blob/master/LICENSE "LICENSE") | [Update LICENSE](https://github.com/pedropro/TACO/commit/43780ca0dd21a165e4ee32ca30dbaf5618bbf5af "Update LICENSE") | 7 years agoAug 21, 2019 |
| [README.md](https://github.com/pedropro/TACO/blob/master/README.md "README.md") | [README.md](https://github.com/pedropro/TACO/blob/master/README.md "README.md") | [get rid of legacy broken pycocotools](https://github.com/pedropro/TACO/commit/91d78047b4a1bbef47b7b12433b11279ba5668b3 "get rid of legacy broken pycocotools") | 4 days agoOct 20, 2026 |
| [demo.ipynb](https://github.com/pedropro/TACO/blob/master/demo.ipynb "demo.ipynb") | [demo.ipynb](https://github.com/pedropro/TACO/blob/master/demo.ipynb "demo.ipynb") | [Fix pandas sort\_values positional args incompatible with pandas 2.x](https://github.com/pedropro/TACO/commit/0e854d04ca861d285aef3b385d342bd712138e3f "Fix pandas sort_values positional args incompatible with pandas 2.x  Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>") | 4 days agoOct 20, 2026 |
| [download.py](https://github.com/pedropro/TACO/blob/master/download.py "download.py") | [download.py](https://github.com/pedropro/TACO/blob/master/download.py "download.py") | [download original JPEG quality](https://github.com/pedropro/TACO/commit/05682d47c0c8401f31b6f3a043333a9c5d96abe1 "download original JPEG quality  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>") | 4 days agoOct 20, 2026 |
| [requirements.txt](https://github.com/pedropro/TACO/blob/master/requirements.txt "requirements.txt") | [requirements.txt](https://github.com/pedropro/TACO/blob/master/requirements.txt "requirements.txt") | [remove old pycocotools](https://github.com/pedropro/TACO/commit/0f5b39e28949ea6e712ee5c60217a116762ce989 "remove old pycocotools") | 4 days agoOct 20, 2026 |
| View all files |

## Repository files navigation

![](https://raw.githubusercontent.com/wiki/pedropro/TACO/images/logonav.png)

TACO is a growing image dataset of waste in the wild. It contains images of litter taken under
diverse environments: woods, roads and beaches. These images are manually labeled and segmented
according to a hierarchical taxonomy to train and evaluate object detection algorithms. Currently,
we have a server that is collecting more images and
annotations @ [tacodataset.org](http://tacodataset.org/)

![](https://raw.githubusercontent.com/wiki/pedropro/TACO/images/1.png)![](https://raw.githubusercontent.com/wiki/pedropro/TACO/images/2.png)![](https://raw.githubusercontent.com/wiki/pedropro/TACO/images/3.png)![](https://raw.githubusercontent.com/wiki/pedropro/TACO/images/4.png)![](https://raw.githubusercontent.com/wiki/pedropro/TACO/images/5.png)

For convenience, annotations are provided in COCO format. Check the metadata here:
[http://cocodataset.org/#format-data](http://cocodataset.org/#format-data)

TACO is still relatively small, but it is growing. Stay tuned!

# Publications

[Permalink: Publications](https://github.com/pedropro/TACO#publications)

For more details check our paper: [https://arxiv.org/abs/2003.06975](https://arxiv.org/abs/2003.06975)

If you use this dataset and API in a publication, please cite us using:

```
@article{taco2020,
    title={TACO: Trash Annotations in Context for Litter Detection},
    author={Pedro F Proença and Pedro Simões},
    journal={arXiv preprint arXiv:2003.06975},
    year={2020}
}
```

# News

[Permalink: News](https://github.com/pedropro/TACO#news)

**December 20, 2019** \- Added more 785 images and 2642 litter segmentations.

**November 20, 2019** \- TACO is officially open for new annotations: [http://tacodataset.org/annotate](http://tacodataset.org/annotate)

# Getting started

[Permalink: Getting started](https://github.com/pedropro/TACO#getting-started)

### Requirements

[Permalink: Requirements](https://github.com/pedropro/TACO#requirements)

To install the required python packages simply type

```
pip3 install -r requirements.txt
```

### Download

[Permalink: Download](https://github.com/pedropro/TACO#download)

To download the dataset images simply issue

```
python3 download.py
```

Alternatively, download from [![DOI](https://camo.githubusercontent.com/ab94931aaa0ac14d52181d6d7bc5ec8626ccfe81d364462903992a8c10f23084/68747470733a2f2f7a656e6f646f2e6f72672f62616467652f444f492f31302e353238312f7a656e6f646f2e333538373834332e737667)](https://doi.org/10.5281/zenodo.3587843)

Our API contains a jupyter notebook `demo.pynb` to inspect the dataset and visualize annotations.

**Unlabeled data**

A list of URLs for both unlabeled and labeled images is now also provided in `data/all_image_urls.csv`.
Each image contains one URL for each original image (second column) and one URL for a VGA-resized version (first column)
for images hosted by us. If you decide to annotate these images using other tools, please make them public and contact us so we can keep track.

**Unofficial data**

Annotations submitted via our website are added weekly to `data/annotations_unofficial.json`. These have not yet been been reviewed by us -- some may be inaccurate or have poor segmentations.
You can use the same command to download the respective images:

```
python3 download.py --dataset_path ./data/annotations_unofficial.json
```

### Trash Detection

[Permalink: Trash Detection](https://github.com/pedropro/TACO#trash-detection)

The implementation of [Mask R-CNN by Matterport](https://github.com/matterport/Mask_RCNN) is included in `/detector`
with a few modifications. Requirements are the same. Before using this, the dataset needs to be split. You can either donwload our [weights and splits](https://github.com/pedropro/TACO/releases/tag/1.0) or generate these from scratch using the `split_dataset.py` script to generate
N random train, val, test subsets. For example, run this inside the directory `detector`:

```
python3 split_dataset.py --dataset_dir ../data
```

For further usage instructions, check `detector/detector.py`.

As you can see [here](http://tacodataset.org/stats), most of the original classes of TACO have very few annotations, therefore these must be either left out or merged together. Depending on the problem, `detector/taco_config` contains several class maps to target classes, which maintain the most dominant classes, e.g., Can, Bottles and Plastic bags. Feel free to make your own classes.

![](https://raw.githubusercontent.com/wiki/pedropro/TACO/images/teaser.gif)![teaser.gif](https://raw.githubusercontent.com/wiki/pedropro/TACO/images/teaser.gif)[Open teaser.gif in new window](https://raw.githubusercontent.com/wiki/pedropro/TACO/images/teaser.gif)

## About

🌮 Trash Annotations in Context Dataset Toolkit

[tacodataset.org](http://tacodataset.org/)

### Topics

[dataset](https://github.com/topics/dataset) [deep-learning](https://github.com/topics/deep-learning) [garbage](https://github.com/topics/garbage) [litter](https://github.com/topics/litter) [mask-rcnn](https://github.com/topics/mask-rcnn) [object-detection](https://github.com/topics/object-detection) [trash](https://github.com/topics/trash)

### Resources

[Readme](https://github.com/pedropro/TACO#readme-ov-file)

[MIT license](https://github.com/pedropro/TACO#MIT-1-ov-file)

[Activity](https://github.com/pedropro/TACO/activity)

### Stars

**775** stars

### Watchers

**27** watching

### Forks

[**251** forks](https://github.com/pedropro/TACO/forks)

[Report repository](https://github.com/contact/report-content?content_url=https%3A%2F%2Fgithub.com%2Fpedropro%2FTACO&report=pedropro+%28user%29)

## Releases

## Packages

## Used by

## Contributors

## Languages

You can’t perform that action at this time.