# NOTE: patch 0213 is intentionally NOT applied here.
# AMI's phosphor-user-manager already incorporates the same getChannelFromIP/
# getUserInfo fixes (loopValue, channelNo + 1, userChannelAccess[channelNo])
# via patches 0242 and others. Applying 0213 after those would fail because
# the context no longer matches.
