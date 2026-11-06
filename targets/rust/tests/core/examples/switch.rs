pub fn dayKind(day: i64) -> i64 {
    let mut label: i64 = 0;
    loop {
        match label {
            0 => {
                if day == 0 {
                    label = 1;
                    continue;
                } else if day == 6 {
                    label = 1;
                    continue;
                } else if day == 1 {
                    label = 2;
                    continue;
                } else if day == 2 {
                    label = 2;
                    continue;
                } else if day == 3 {
                    label = 2;
                    continue;
                } else if day == 4 {
                    label = 2;
                    continue;
                } else if day == 5 {
                    label = 2;
                    continue;
                } else {
                    label = 3;
                    continue;
                }
            }
            1 => {
                return 0;
            }
            2 => {
                return 1;
            }
            3 => {
                return 0 - 1;
            }
            _ => unreachable!(),
        }
    }
}
