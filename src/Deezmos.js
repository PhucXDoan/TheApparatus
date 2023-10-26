const run = () => {
	let dummy_input = document.createElement("input");
	dummy_input.type = "file";
	dummy_input.onchange = ({ target: { files: [dummy_blob] } }) =>
	{
		let dummy_reader = new FileReader();
		dummy_reader.readAsText(dummy_blob, "UTF-8");
		dummy_reader.onload = ({ target: { result: file_content }}) =>
		{
			let json  = JSON.parse(file_content);
			let state = Calc.getState();

			if ("avg_screenshot_rgbs" in json)
			{
				const image_name_folder_id = Math.random().toString();

				state.graph.squareAxes = false;
				state.graph.viewport.xmin = -1;
				state.graph.viewport.ymin = -0.1;
				state.graph.viewport.xmax = 14;
				state.graph.viewport.ymax = 1.1;

				state.expressions =
					{
						list:
							[
	//							{
	//								type: "expression",
	//								latex: `P = \\left[${slots_of_interest_indices}\\right]`
	//							},
	//							{
	//								type: "expression",
	//								latex: `P_n = \\left[${new Array(WORDBITES_BOARD_SLOTS_X * WORDBITES_BOARD_SLOTS_Y).fill(null).map((_, i) => i).filter(x => slots_of_interest_indices.indexOf(x) == -1)}\\right]`
	//							},
								{
									type: "expression",
									color: "#FF0000",
									latex: `y = \\operatorname{mean}\\left(R\\right)`
								},
								{
									type: "expression",
									color: "#00FF00",
									latex: `y = \\operatorname{mean}\\left(G\\right)`
								},
								{
									type: "expression",
									color: "#0000FF",
									latex: `y = \\operatorname{mean}\\left(B\\right)`
								},
								{
									type: "table",
									columns:
										[
											{ values: json.avg_screenshot_rgbs.map((_, i) => i.toString()), latex: "I" },
											{ values: json.avg_screenshot_rgbs.map(screenshot => screenshot.avg_rgb.r.toString()), color: "#FF0000", latex: "R" },
											{ values: json.avg_screenshot_rgbs.map(screenshot => screenshot.avg_rgb.g.toString()), color: "#00FF00", latex: "G" },
											{ values: json.avg_screenshot_rgbs.map(screenshot => screenshot.avg_rgb.b.toString()), color: "#0000FF", latex: "B" },
										]
								},
								{
									type:      "folder",
									title:     "Image names.",
									collapsed: true,
									id:        image_name_folder_id,
								},
								...json.avg_screenshot_rgbs.map((screenshot, i) => ({
										type:             "expression",
										color:            "#000000",
										label:            screenshot.name,
										labelAngle:       "-\\frac{\\pi}{4}",
										labelOrientation: "right",
										labelSize:        "0.85",
										showLabel:        true,
										hidden:           true,
										folderId:         image_name_folder_id,
										latex:            `\\left(${i}, \\frac{7}{8}\\operatorname{min}\\left(R, G, B\\right)\\left[${i + 1}\\right]\\right)`
									})
								),
	//							{
	//								type: "expression",
	//								color: "#FF0000",
	//								pointStyle: "OPEN",
	//								latex: `\\left(P, R\\left[P + 1\\right]\\right)`
	//							},
	//							{
	//								type: "expression",
	//								color: "#00FF00",
	//								pointStyle: "OPEN",
	//								latex: `\\left(P, G\\left[P + 1\\right]\\right)`
	//							},
	//							{
	//								type: "expression",
	//								color: "#0000FF",
	//								pointStyle: "OPEN",
	//								latex: `\\left(P, B\\left[P + 1\\right]\\right)`
	//							}
							]
					};
			}

			Calc.setState(state);
		};
	};
	dummy_input.click();
};
