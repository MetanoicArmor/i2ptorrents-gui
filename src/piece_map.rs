//! Piece-map column aggregation used by the Qt `PieceMap` widget.

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PieceFill {
    Empty,
    Partial,
    Have,
}

pub fn piece_column_fill(have: &[bool], width: usize) -> Vec<PieceFill> {
    if width == 0 {
        return Vec::new();
    }
    let total = have.len();
    (0..width)
        .map(|column| {
            if total == 0 {
                return PieceFill::Empty;
            }
            let start = column * total / width;
            let end = (start + 1).max((column + 1) * total / width);
            let chunk = &have[start..end.min(total)];
            if chunk.is_empty() {
                PieceFill::Empty
            } else if chunk.iter().all(|&bit| bit) {
                PieceFill::Have
            } else if chunk.iter().any(|&bit| bit) {
                PieceFill::Partial
            } else {
                PieceFill::Empty
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_have_is_empty_columns() {
        assert_eq!(
            piece_column_fill(&[], 4),
            vec![
                PieceFill::Empty,
                PieceFill::Empty,
                PieceFill::Empty,
                PieceFill::Empty
            ]
        );
    }

    #[test]
    fn all_have_fills_every_column() {
        assert!(piece_column_fill(&[true, true, true, true], 8)
            .iter()
            .all(|fill| *fill == PieceFill::Have));
    }

    #[test]
    fn mixed_bits_produce_partial_columns() {
        let fills = piece_column_fill(&[true, false], 1);
        assert_eq!(fills, vec![PieceFill::Partial]);
    }
}
